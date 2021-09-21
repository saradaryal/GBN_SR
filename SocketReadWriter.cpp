#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


using namespace std;

//The header that includes all necessary data
//For the receiver to preempt for the actual packet data.
typedef struct Header {
	int id, length;
	short checksum;
} Header;

//A struct used to better handle complete packet data.
//Used on both sides
//The bool "secured" is true if the packet was successfully transmitted
typedef struct Packet {
	int id, length;
	short checksum;
	//Transmitted indicates whether or not the data was sent in any way. Dropped, corrupted, normal, doesn't matter
	//Secured indicates whether or not the data was not only sent, but confirmed to have been received uncorrupted.
	//Terminated indicates (if true) that this packet has been rendered unnecessary and should not be sent for any reason.
	bool transmitted, secured, terminated;
	char* content;
} Packet;


void shiftWindow(int shiftValue, int windowSize, int sequenceRange, Packet* packets); //Shifts the packets in then given array by the given amount
bool feignError(int id, int numDrops, int* drops, int windowSize, int sequenceRange, int lowID, bool* alreadyDone); //Determines if an error should be simulated based on the given data
short inetChecksum(char* bytes, int length); //Creates a checksum value to determine the integrity of the given data


//Class made for handling reading and writing through datagram sockets
class SocketReadWriter {
	private:
		//The socket file descriptor
		int sockfd;
	
		//The array of bytes acting as the buffer, as well as the length of said buffer.
		int bufferSize;
		char* buffer;
	
		//The details of the other side of the connection, as well as how large the  is.
		sockaddr_in *destination, *home;
		socklen_t destSize, homeSize;
		
		//Basic method for reading data from a connection. All other reading methods should use this.
		//Returns true if data was successfully obtained, false if a timeout happened instead.
		bool readData(char* saveHere, size_t bytes) {
			size_t bytesRead = 0;
			while (bytesRead < bytes) {

                ssize_t bytesThisTime = recvfrom(sockfd, saveHere + bytesRead, bytes - bytesRead, 0,(sockaddr*) destination, &destSize);
				if (bytesThisTime == -1) return false;

                bytesRead += bytesThisTime;
				
				//If that wasn't everything that was expected, let the sender know this object is ready for more.
				if (bytesRead < bytes) {
					signalReady();
		        }
			}
			return true;
		}
		
		//Basic method for writing data through a connection. All other writing methods should use this.
		//Returns true if the transfer was successful, false if something blocked it.
		bool sendData(char* data, size_t bytes) {
			size_t bytesSent = 0;
			while (bytesSent < bytes) {
				ssize_t bytesThisTime = sendto(sockfd, data + bytesSent, bytes - bytesSent, 0, (sockaddr*) destination, destSize);

                if (bytesThisTime == -1) return false;

                bytesSent += bytesThisTime;
				
				//If not everything sent, wait for the receiver to be ready
				if (bytesSent < bytes) {
					waitReady();
				}
			}
			return true;
		}
		
		//Constructor. This should only be invoked via the static method getInstance, seen at the bottom of the class.
		SocketReadWriter(int sock,  sockaddr_in* connectionInfo, sockaddr_in* localInfo, int bufferLength) {
			//Save the socket filedescriptor
			sockfd = sock;
			//Allocate a buffer of appropriate size.
			buffer = new char[bufferSize = (bufferLength + sizeof(Header))];
			
			//Get address info and size of the destination
			destination = connectionInfo;
			destSize = sizeof(*destination);
			
			//Also save the local information, in case it's necessary to keep in memory.
			home = localInfo;
			homeSize = sizeof(*home);
		}
	
	public:
		//Obtains an integer from the connection, returning it instead of saving it to the buffer
		//NOTE: If this function returns -3, that's a timeout indicator, not a real result.
		int getInt() {
			int send;
			bool worked = readData((char*) &send, sizeof(send));
			return worked ? send : -3;
		}
		
		//Sends an given integer through the connection instead of using the buffer data
		//Returns true if successful, false if timed out
		bool sendInt(int value) {
			return sendData((char*) &value, sizeof(value));
		}
		
		//Shortcut function for calculating packet checksum
		short checksum() {
			return inetChecksum(buffer, bufferSize);
		}


		//Gets a header from the socket and returns its value
		//NOTE: if this header has an ID of -3, that's a timeout indicator, not a real header.
		Header getHeader() {
			Header send;
			if (!readData((char*) &send, sizeof(send))) send.id = send.length = -3;
			return send;
		}

		//Sends a header with the given id. All other data is calculated from what is already inside this class
		//Returns true if successful, false if timed out
		bool sendHeader(int id) {
			Header head;
			head.id = id;
			head.length = bufferSize;
			head.checksum = checksum();

			return sendData((char*) &head, sizeof(head));
		}

		
		//Deliberately stalls execution until given a single byte, to indicate that the other side is ready to receive info.
		//Returns true if successful, false if timed out
		bool waitReady() {
			char c = '\0';
			return readData(&c, 1);
		}
	
		//Gives a single byte to the other side, indicating that this side is ready to receive info.
		//Returns true if successful, false if timed out
		bool signalReady() {
			char c = '\0';
			return sendData(&c, 1);
		}
		
		//Copies the data currently in the buffer and returns it as a newly allocated char array.
		//Given that this array is a copy, any changes to it will not affect this object's buffer.
		//The value at the given int pointer will change to the size of the buffer.
		//Make sure to free the returned array once you are done with it.
		char* getCopyofData(int* length){ 
			char* send = new char[bufferSize];

			for (int i = 0; i < bufferSize; i++) {
				send[i] = buffer[i];
			}
			if (length != NULL) *length = bufferSize;
			return send;
		}
		
		
		char* parseData(Header* head, int sequenceRange) {
			*head = *((Header*) buffer);
			
			//If it's logically impossible for this to be an intact packet, don't return anything.
			if (head->id < 0 || head->id >= sequenceRange || head->length < 0 || head->length > (bufferSize - sizeof(Header))) return NULL;
			
			char* send = new char[head->length];
			
			for (int s = 0, i = sizeof(Header); s < head->length; s++) {
				send[s] = buffer[i++];
			}
			
			return send;
		}
		
		
		//Sets the buffer data to be a copy of the given data
		void setData(char* data, int length) {
			if (length != bufferSize) {
				if (buffer != NULL) delete[] buffer;
				
				buffer = length == 0 ? NULL : new char[length];
                bufferSize = length;
			}
			if (data != NULL) {
				for (int i = 0; i < length; i++) {
					buffer[i] = data[i];
				}
			}
		}
		
		void setPacket(char* data, int length, int id) {
			Header* header = (Header*) buffer;
			header->length = length;
			header->id = id;
			header->checksum = inetChecksum(data, length);
			
			char* spot = (char*) (header + 1);
			
			for (int i = 0; i < length; i++) {
				spot[i] = data[i];
			}
		}

		//Reads from the socket and loads the data into the buffer
		//Returns true if successful, false if timed out
		bool getPacket() {
			return readData(buffer, bufferSize);
		}
		
		//Takes the data in the buffer and sends it through the socket.
		//Returns true if successful, false if timed out
		bool sendPacket() {
			return sendData(buffer, bufferSize);
		}


		//Configures the timeout of the socket, given a time in full seconds plus additional microseconds
		//Returns true if successful, false if not.
		bool setTimeout(int fullSeconds, int plusMicroSeconds) {
			struct timeval time;
			time.tv_sec = fullSeconds;
			time.tv_usec = plusMicroSeconds;
			return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&time,sizeof(time)) >= 0;
		}
	
		//Destructor. Closes the socket it contained and frees any dynamically allocated data.
		~SocketReadWriter() {
			close(sockfd);
			if (buffer != NULL) delete[] buffer;
			delete destination;
			delete home;
		}


		//Changes the port that this object will expect from the other side of the connection.
		//You should generally ONLY use this if you're doing a LOCALHOST run.
		void setOtherSidePort(int port) {
			destination->sin_port = htons(port);
		}
		
		//Given an ip address, a port, and a default buffer size, this function
		//creates a SocketReadWriter object. Returns the address of the object if successful, or NULL if not.
		static SocketReadWriter* getInstance(string* ip, int port, int bufferSize, int timeoutSeconds, int timeoutMicroSeconds) {
				if (bufferSize < 1 || timeoutMicroSeconds < 0 || timeoutMicroSeconds < 0) return NULL;

				//If the user said to do localhost, just do that.
				bool local = ip->compare("localhost") == 0;
				
				//The connectionInfo will be the info used to send data. localInfo will be used for socket binding.
				sockaddr_in *connectionInfo = new sockaddr_in(), *localInfo = new sockaddr_in();
				connectionInfo->sin_family = localInfo->sin_family = AF_INET;
				
				//Parse the ip of the other side from that string (or just do localhost), returning null if it fails
				if ((connectionInfo->sin_addr.s_addr = local ? htonl(INADDR_ANY) : inet_addr(ip->c_str())) < 0) {
					delete connectionInfo;
					delete localInfo;
					return NULL;
				}
				
				//The local info will set up itself, no input from the user.
				//First, get the name of the host
				char host[256];
				int hostname = gethostname(host, sizeof(host));
	
				//Then, get the information of said host as something we can use.
				struct hostent* host_entry = gethostbyname(host); 
	
				//And then, get the ip address as a string
				char* myIp = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
				
	
				//And then turn that string into the value we need to work (unless we're doing localhost anyway)
				localInfo->sin_addr.s_addr = local ? htonl(INADDR_ANY) : inet_addr(myIp);
				
				//Now the ip address is set for socket binding later
				
				
				//Now, add the port for both the destination and the local structs
				connectionInfo->sin_port = localInfo->sin_port = htons(port);
				
				//Try to make a socket. If it fails, stop
				int sock = socket(AF_INET, SOCK_DGRAM, 0);
				if (sock < 0) {
					delete connectionInfo;
					delete localInfo;
					return NULL;
				}


				//If the user specified a timeout interval, attempt to set it up. If it fails, stop.
				if (timeoutSeconds > 0 || timeoutMicroSeconds > 0) {
					struct timeval time;
					time.tv_sec = timeoutSeconds;
					time.tv_usec = timeoutMicroSeconds;
					if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&time,sizeof(time)) < 0) {
   						close(sock);
						delete connectionInfo;
						delete localInfo;
						return NULL;
					}
				}
				
				
				//Bind the socket with our local info. If that fails, stop
				if (bind(sock,(const sockaddr*) localInfo, sizeof(*localInfo)) < 0) {
					close(sock);
					delete connectionInfo;
					delete localInfo;
					return NULL;
				}
				
				//If everything worked, return a new instance of the read-writer
				return new SocketReadWriter(sock, connectionInfo, localInfo, bufferSize);
		}

		static SocketReadWriter* getInstance(string* ip, int port, int bufferSize) {
			return getInstance(ip, port, bufferSize, 0, 0);
		}

};






//Rotates the packets of the window a given amount of times.
//Then changes the ID numbers to match the new order
void shiftWindow(int shiftValue, int windowSize, int sequenceRange, Packet* packets) {
	//No point in shifting if we're not actually supposed to do it.
	if (shiftValue == 0) return;
	
	//If not every packet was successful, rearrange the ones that were to the back.
	//(Up until the first unsuccessful one is at the start of the array)
	if (shiftValue != windowSize) {
		
		for (int i = 0, last = windowSize - 1; i < shiftValue; i++) {
			Packet pack = packets[0];
		
			for (int j = 1; j < windowSize; j++) {
				packets[j-1] = packets[j];
			}
		
			packets[last] = pack;
		}
		
		//Update the id numbers according to their new positions
		for (int i = 1; i < windowSize; i++) {
			packets[i].id = (packets[i-1].id + 1) % sequenceRange;
		}
	}
	//If all of them were successful, we can just update the indicators instead of actually moving the packets
	else {
		for (int i = 0; i < windowSize; i++) {
			packets[i].id = (packets[i].id + windowSize) % sequenceRange;
			packets[i].secured = packets[i].transmitted = false;
			packets[i].checksum = -1;
		}
	}
}

//Determines if an error with a given pacet should be simulated. This can be used for either dropping acks or packets
//id is the id number of the packet (or ack) currently being checked
//numDrops is the number of ids listed in the array "drops". A value of -1 triggers random generation instead of a lookup of the list.
//drops is the list of ids to drop
//alreadyDone is an array of the same size as drops, indicating which indexes were already used that thus shouldn't be used again.
//--The remaining variables are for random number generation
//windowSize is the size of the window, to modulate the initial random number
//sequenceRange represents the range of possible id numbers (0 - the exclusive bound of sequenceRange)
//lowID is the id number of the first packet in the window, to ensure that the id generated is within the bounds of the actual window
bool feignError(int id, int numDrops, int* drops, int windowSize, int sequenceRange, int lowID, bool* alreadyDone)  {
	//If we're supposed to generate a random number
	if (numDrops == -1) {
		//Generate an ID number within the bounds of the current window
		int randID = ((rand() % (windowSize < 2 ? 25 : windowSize)) + lowID) % sequenceRange;
		return id == randID;
	}
	//If we're actually supposed to look for an id in the list, do so
	else {
		for (int i = 0; i < numDrops; i++) {
			//If the id at this index hasn't been used and matches the target id, return true
			if (!alreadyDone[i] && id == drops[i]) {
				alreadyDone[i] = true;
				return true;
			}
		}
		//If no unused matches were found, return false.
		return false;
	}
}


//Uses inernet checksum to identify a set of bytes.
//You'll know if a packet kept its integrity if the short the sender gave
//Is the same as the short the receiver calculates.
short inetChecksum(char* bytes, int length) {
	//All one's complement addition results will be put here
	unsigned short send = 0;
	
	if (bytes == NULL) return send;
	//For every byte in the given array
	for (int i = 0, shortSize = sizeof(send), key = 1 << (shortSize * 8); i < length; i += shortSize) {
		unsigned short next = 0;
		char* nextBytes = (char*) &next;
		//Parse a short's worth of bytes into a variable 
		//(letting bytes of 0 be placeholders if there aren't enough "real" bytes left for an entire short.)
		for (int j = 0; j < shortSize && (j + i) < length; j++) {
			nextBytes[j] = bytes[j + i];
		}
		//Do one's complement addition, adding any carryover to the least significant bit
		int sum = ((int) send) + next;
		if (sum & key) sum++;
		//Condense back into a short now that overflow is handled
		send = sum & (key-1);
	}
	//Invert the bits and return whatever results
	return ~send;
}