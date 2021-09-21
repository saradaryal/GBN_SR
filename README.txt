CREATED BY Daniel Ott and Sarad Aryal


This project relays files from one location to another using a server-client system that utilizes either Go-Back-N, or Selective Repeating.

LIST OF FILES
ottdc6030_aryals9686_main.cpp - The main file, which handles the main menu and post-transfer statistics

ottdc6030_aryals9686_server.cpp - The file that handles the server functions, collecting data from a client and writing it to a file.

ottdc6030_aryals9686_client.cpp - The file that handles the client functions, reading data from a file and sending it to a server.

ottdc6030_aryals9686_SocketReadWriter.cpp - The file that holds the functions, structs, and the titular socket-handling class that are shared by both the server and client.

ottdc6030_aryals9686_LinkedList.cpp - The file that contains a linked list class (and nodes for said class) containing packets, for use in GBN server functions.

ottdc6030_aryals9686_makefile - The makefile that will compile this program. It's heavily reccomended that you use this to compile.

README.txt - This file.

HOW TO COMPILE
This program was developed on the phoenix servers and in local linux environments.

To compile, one should run the makefile by running the command "make -f ottdc6030_aryals9686_makefile"
The result should be two programs called ottdc6030_aryals9686_server.exe and ottdc6030_aryals9686_client.exe.



HOW TO USE
Both the server and client will have menus that will guide you through the setup process. 
However, you should make sure that the client has one accessible file to transmit, and you should make sure the SERVER is fully set up BEFORE setting up the client.

Both will ask the following questions:
-Whether you want to use Go-Back-N (GBN) or Selective Repeating (SR) to transmit? A simple GBN or SR will work for an answer.

-The size of every packet in bytes. Providing a nonzero number will do

-How should the connection know when to time out, will you manually give a number of seconds (Manual), or should the program figure it out itself (Ping)?
	-NOTE: If you answer "Manual", then the program will ask you for a number of whole seconds to wait when handling timeouts.

-What should be the size of the sliding window? In other words, how many packets should the program handle at a time?
	-NOTE: If the server is running GBN, it won't ask this question, due to the nature of GBN.

-Packets are given an ID for verification. What should the maximum (exclusive) upper bound of those IDs be?
	-NOTE: For stability's sake, choose a number that is larger than the number you chose for the sliding window size.

-Should errors be simulated by dropping data? You can choose either... 
	-"Yes" to customize the simulated errors, "Random" to let the server randomize the errors, or "No" for no errors at all.
	-NOTE: If you choose "Yes", you will be asked to enter a list of ID numbers, separated by spaces. If you want one dropped multiple times, enter it that many times.
	
-What is the IP address and port of the other side of this connection?
	-NOTE: If you need to run locally, you can enter "localhost" in place of an IP address. You won't be asked for a port in this case.
	
-Lastly, the program will ask for a file name.
	-If this is the server, it will write a file with this name, overwriting the current file of the same name if there is one.
	-If this is the client, it will read from a file of that name, but said file must exist if you want to continue.
	
Once both sides have their answers, the programs will use datagram sockets to transfer the file over, displaying statistics and an MD5sum value afterwards.
You'll know if the file was perfectly transferred if the MD5sum value is the same on both sides.


COMPLICATIONS
Error handling on GBN is not completely reliable, sometimes resulting in packets not making it to the file or repeated packets printed to file.
Ping-based timeout calculation does have issues when packets and window sizes become too large, causing the two programs to misalign and/or crash.
The post-transmission statistics (at least server-side) struggle to determine the difference between an original packet and a re-sent one, causing innacurate numbers.
