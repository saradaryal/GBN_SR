

//Uses GO-Back-N to send file data through the socket.
long* GBN(SocketReadWriter* sock, FILE* file, int packetSize, int windowSize, int sequenceRange, int numDropAcks, int* dropAcks);

//Uses Selective Repeating to write file data to the socket
long* selectRepeat(SocketReadWriter* sock, FILE* file, int packetSize, int windowSize, int sequenceRange, int numDropAcks, int* dropAcks);


//Loads packets of data from the file, returning true if the last of the file data has been collected.
bool packetsFromFile(int startIndex, int windowSize, int packetSize, Packet* packets, FILE* file);

//returns true if all packets listed have been listed as successfully transferred
bool allDone(Packet* packets, int windowSize);



//Uses Selective Repeating to write file data to a socket
//sock is the read-writer class used to handle socket data
//file is the file in question
//packetSize is the size in bytes of each individual packet.
//windowSize indicates how many packets there are in the window
//sequenceRange is the maximum exclusive bound of the sequence ids (the inclusive min is 0)
//numDropAcks is the length of the list of ids to drop (dropAcks) as part of error simulation.
//Returns an array of longs for use in post-function statistsics
long* selectRepeat(SocketReadWriter* sock, FILE* file, int packetSize, int windowSize, int sequenceRange, int numDropAcks, int* dropAcks) {

	long* send = new long[4];
	for (int i = 0; i < 4; i++) {
		send[i] = 0L;
	}


	//If error simulation is being used, keep track of which ids have already been dropped, to avoid softlocking the program.
	bool* alreadyDone = numDropAcks > 0 ? new bool[numDropAcks] : NULL;
	for (int i = 0; i < numDropAcks; i++) {
		alreadyDone[i] = false;
	}

	//Initialize the packet structs
	cout << "Intitializing packets\n";
	Packet packets[windowSize];
	for (int i = 0; i < windowSize; i++) {
		Packet* pack = packets + i;
		pack->secured = pack->transmitted = pack->terminated = false;
		pack->id = i % sequenceRange;
		pack->length = packetSize;
		pack->content = new char[packetSize];
		pack->checksum = -1;
	}

	bool noMoreFileData = false, firstRun = true;
	//Until we run out of data to send
	while (!(noMoreFileData && allDone(packets, windowSize))) {
		//Figure out how much we can shift the window
		//Any securely sent packets can be kicked to the back
		int shiftValue = 0;
		while (shiftValue < windowSize && packets[shiftValue].secured) shiftValue++;
		
		//If the variables need to be shifted around (or this is the first run of the cycle) 
		if (shiftValue > 0 || firstRun) {
			cout << (firstRun ? "First run through\n" : "Shifting packets\n");
			firstRun = false;
			//Shift the window however many spaces we need
			shiftWindow(shiftValue, windowSize, sequenceRange, packets);
			cout << "Reading from file\n";
			//Load file data into the appropriate packets (if needed)
			if (!noMoreFileData) {
				int start = shiftValue == 0 ? 0 : windowSize-shiftValue;
				noMoreFileData = packetsFromFile(start, windowSize, packetSize, packets, file);
			}
		}

		//For every packet in the window
		for (int i = 0; i < windowSize; i++) {
			Packet* pack = packets + i;
			//Don't bother with a packet that has already been properly received
			if (pack->secured) continue;
			
			//Prime the read-writer for sending
			//sock->setData(pack->content, pack->length);
			//sock->waitReady();
			sock->setPacket(pack->content, pack->length, pack->id);
			
			cout << (pack->transmitted ? "Retransmitting" : "Sending") << " packet of id: " << pack->id << endl;
			
			//Once ready, send the packet over;
			if (!sock->sendPacket()) {
				cout << "Packet of id " << pack->id << " failed\n";
				continue;
			}

			if (pack->transmitted) {
				send[1]++;
				send[3] += pack->length;
			}

			send[0]++;
			send[2] += pack->length;

			//Indicate that the packet has been sent, but not that it's been verified
			pack->transmitted = true;
			
		}

		cout << "All packets sent, waiting for server to be ready to send acks\n";
		while (!sock->waitReady());
		sock->signalReady();

		cout << "Server ready, waiting for acks\n";

		//The receiver is going to relay the IDs of the successful packets
		int relayed[windowSize], relaySize = 0, acked;
		//Until timeout, keep adding ints
		while ((acked = sock->getInt()) != -3) {
			//Check to see if we should simulate an error (pretend we failed to get this ack). If not, add it to the list
			if (!feignError(acked, numDropAcks, dropAcks, windowSize, sequenceRange, packets[0].id, alreadyDone)) {
				cout << "Obtained ack for packet id " << acked << endl;
				relayed[relaySize++] = acked;
			}
		}
		//printing window content
				int i = 0;
				 cout << "Current Window: [";
				for(i = 0; i < windowSize-1; i++){
					cout << packets[i].id << ", " ;
				}
				cout << packets[windowSize -1].id << "]"<< endl;

		cout << "Timed out, for waiting for server to be ready for returned acks\n";

		sock->signalReady();
		while (!sock->waitReady());

		cout << "Server ready, returning obtained acks\n\n";

		for (int i = 0; i < relaySize; i++) {
			int index = 0;
			while (packets[index].id != relayed[i]) index++;
			packets[index].secured = true;
			sock->sendInt(relayed[i]);
		} 

		//cout << "Waiting for ready before sending next header\n\n";

		while (!sock->waitReady());
	}

	//Free the allocated data we no longer need
	for (int i = 0; i < windowSize; i++) {
		if (packets[i].content != NULL) delete[] packets[i].content;
	}
	
	//cout << "Sending quit signal\n\n";


	//Get rid of error-tracking array we don't need anymore
	if (alreadyDone != NULL) delete[] alreadyDone;
	
	//Send a packet entirely composed of 0's to indicate that we're done.
	//sock->setData(NULL,0);
	//sock->sendHeader(0);

	return send;
}


//Loads file data into the window. Returns true if the last of the file data has been read.
bool packetsFromFile(int startIndex, int windowSize, int packetSize, Packet* packets, FILE* file) {
	int cutoff = windowSize;
	bool send = false;
	
	for (int i = startIndex; i < windowSize; i++) {
		//load the data into the packet
		int bytesRead = fread(packets[i].content, 1, packetSize, file);
		
		//cout << "Read " << bytesRead << "/" << packetSize << " bytes from file, checksum value = " << inetChecksum(packets[i].content, bytesRead) <<"\n";
		packets[i].secured = packets[i].transmitted = false;
		
		//If the data was smaller than expected (indicating the file is done being read)
		if (bytesRead < packetSize) {
			//Close the file
			fclose(file);
			
			//Change the length
			packets[i].length = bytesRead;
			
			//Set the cutoff value to either this index or the next,
			//Depending on whether or not this packet actually got data
			cutoff = bytesRead == 0 ? i : i+1;
			send = true;
			//Stop the current cycle
			break;
		}
	}
	
	//For every packet we know is not going to be used.
	for (int i = cutoff; i < windowSize; i++) {
		//Free the data that won't be used
		if (packets[i].content != NULL) delete[] packets[i].content;
		packets[i].content = NULL;
		//Pretend the packet has already been sent so it doesn't get used
		packets[i].secured = packets[i].terminated = true;
		packets[i].transmitted = false;
		packets[i].checksum = -1;
	}
	
	//If we reached the end of the file, send true to indicate that these packets are the last.
	return send;
}


//Returns true if each and every packet has been securely sent, false if not.
bool allDone(Packet* packets, int windowSize) {
	bool send = true;
	for (int i = 0; send && i < windowSize; i++) {
		send = packets[i].secured;
	}
	return send;

}


//Uses GO-Back-N to send file data through a socket.
//sock is the read-writer class used to handle socket data
//file is the file in question
//packetSize is the size in bytes of each individual packet.
//windowSize indicates how many packets there are in the window
//sequenceRange is the maximum exclusive bound of the sequence ids (the inclusive min is 0)
//numDropAcks is the length of the list of ids to drop (dropAcks) as part of error simulation.
//Returns an array of longs for use in post-function statistsics
long* GBN(SocketReadWriter* sock, FILE* file, int packetSize, int windowSize, int sequenceRange, int numDropAcks, int* dropAcks){
	long* send = new long[4];
	for (int i = 0; i < 4; i++) {
		send[i] = 0L;
	}
	
	//Initialize packet structs
	Packet packets[windowSize];
	for(int i = 0; i < windowSize; i++){
		packets[i].secured = packets[i].transmitted = packets[i].terminated = false;
		packets[i].id = i % sequenceRange;
		packets[i].length = packetSize;
		packets[i].content = new char[packetSize];
		packets[i].checksum = -1;

	}

	//Any ids listed to be dropped are only dropped once per entry. This makes sure no unneeded repeats are made.
	bool* alreadyDone = numDropAcks < 1 ? NULL : new bool[numDropAcks];
	for (int i = 0; i < numDropAcks; i++) {
		alreadyDone[i] = false;
	}


    bool first = true, last = false;

	//Until all data has been sent
    while (!(last && allDone(packets, windowSize))) {
        
		//Find out which packets need to be sent.
		int shiftValue = 0;
        while (shiftValue < windowSize && packets[shiftValue].secured) shiftValue++;
        
		//If there are some (or this is the first run)
		if (shiftValue > 0 || first){
        	first = false;
			cout << "Shifting window"<< endl;
        	shiftWindow(shiftValue, windowSize, sequenceRange, packets);
        	
			//If the data isn't done, load packets from the file.
			if (!last){
            	int start = shiftValue == 0 ? 0 : windowSize - shiftValue;
				cout << "Loading the window" << endl;
            	last = packetsFromFile(start, windowSize, packetSize, packets, file);
        	}
        }

		cout << "Loaded window, sending....\n";
        for (int i = 0; i < windowSize; i++){
            Packet *pack = packets +i;
            //If a packet has been marked as unneeded, don't send it.
			if (pack->terminated) continue; 
			cout << "Sending packet of id " << pack->id << endl;
            sock->setPacket(pack->content, pack->length, pack->id);
            
			//Send the header data, then the actual packet
			//sock->sendHeader(pack->id);
            sock->sendPacket();

			if (pack->transmitted) {
				send[1]++;
				send[3] += pack->length;
			}

			pack->transmitted = true;
			send[0]++;
			send[2] += pack->length;

        }


		cout << "Sending done, waiting for server to be ready to send acks\n";

		while (!sock->waitReady());
		sock->signalReady();

		cout << "Server ready, receiving acks\n";

		//For every ack we get in return, make sure we don't have to drop it before saving it.
        int send[windowSize], size = 0, ack;
        while((ack = sock->getInt()) != -3){
			if (!feignError(ack, numDropAcks, dropAcks, windowSize, sequenceRange, packets[0].id, alreadyDone)) {
				cout << "Obtained ack of id " << ack << endl;
            	send[size++] = ack;
			}
        }


		int i = 0;
		cout << "Current Window: [";
		for(i = 0; i < windowSize-1; i++){
			cout << packets[i].id << ", " ;
		}
		cout << packets[windowSize -1].id << "]"<< endl;

		cout << "Time out, indicating ready to send copy of acks\n";

		sock->signalReady();
		while (!sock->waitReady());


		//For every ack we just got, mark it's packet as secure
        for (int i = 0; i < size; i++) {
            int point = 0; 
            while(packets[point].id != send[i]) point++;

			//cout << "INDEX FOR ID " << send[i] << ": " << point << endl;
            packets[point].secured = true;

			//cout << "Sending copy of " << send[i] << endl;
           // sock-> sendInt(send[i]);
        }

        bool sarad = true;

		//For every packet
		cout << "Returning copies of acks ";
        for(int i = 0; i < windowSize; i++) {
			//If it's  a secure packet (and we haven't seen a nonsecure packet yet), send its id as a parroted ack
            sarad = sarad && packets[i].secured && packets[i].transmitted;
            if (sarad) {
				cout << packets[i].id << ", ";
				sock->sendInt(packets[i].id);
				//cout << "Retransmitting packet id" << packets[i].id << endl;
			}
        }

		cout << "\nWaiting for server to be ready for next cycle\n\n";
		while (!sock->waitReady());

    }

	if (alreadyDone != NULL) delete[] alreadyDone;

    for (int i = 0; i < windowSize; i ++){
        if (packets[i].content != NULL) delete[] packets[i].content;
		cout << "deallocating the packet content" << endl;
    }
    
	return send;
}
