//Uses Selective Repeating to write socket data to a file.
//sock is the read-writer class used to handle socket data
//file is the file in question
//packetSize is the size in bytes of each individual packet.
//windowSize indicates how many packets there are in the window
//sequenceRange is the maximum exclusive bound of the sequence ids (the inclusive min is 0)
//numDropAcks is the length of the list of ids to drop (dropAcks) as part of error simulation.
//Returns an array of longs for use in post-function statistsics
long* selectRepeat(SocketReadWriter* sock, FILE* file, int packetSize, int windowSize, int sequenceRange, int numDropPacks, int* dropPacks) {
	long* send = new long[3];
	for (int i = 0; i < 3; i++) {
		send[i] = 0L;
	}


	//If error simulation is being used, keep track of which ids have already been dropped, to avoid softlocking the program.
	bool* alreadyDone = numDropPacks > 0 ? new bool[numDropPacks] : NULL;
	for (int i = 0; i < numDropPacks; i++) {
		alreadyDone[i] = false;
	}
	
	//Initialize the packet structs
	cout << "Initializing packets\n";
	Packet packets[windowSize];
	for (int i = 0; i < windowSize; i++) {
		Packet* pack = packets + i;
		pack->secured = pack->transmitted = false;
		pack->id = i % sequenceRange;
		pack->length = packetSize;
		pack->content = NULL;
		pack->checksum = -1;
		pack->terminated = false;
	}


	int timesNothingFound = 0;

	//Until we have gotten all the file data.
	while (true) {
		//Figure out how much we can shift the window
		//Any securely received packets (until the first not-received packet) can be written to file and moved to the back
		int shiftValue = 0;
		while (shiftValue < windowSize && packets[shiftValue].secured) shiftValue++;

		//If the variables need to be shifted around, do so.
		if (shiftValue > 0) {
			//cout << "Shifting packets\n";
			shiftWindow(shiftValue, windowSize, sequenceRange, packets);
			cout << "Writing shifted packets to file\n";
			//For every packet that was shifted to the back, write its data to the file
			for (int i = windowSize - shiftValue; i < windowSize; i++) {
				cout << "WRITING PACKET INDEX " << i << endl;
				fwrite(packets[i].content,1,packets[i].length, file);
				packets[i].transmitted = packets[i].secured = false;
			}
			
		}
		
		bool gotPacket = false;
		
		//Until the user stops getting packets
		while (sock->getPacket()) {
			gotPacket = true;
			Header head;
			char* data = sock->parseData(&head, sequenceRange);
			
			send[0] = head.id;
			send[1]++;
			
			//Check to see if there actually is decent data, and if should simulate an error (Pretend we failed to grab this packet).
			//If the packet checks out, add it to the window
			if (data != NULL && !feignError(head.id, numDropPacks, dropPacks, windowSize, sequenceRange, packets[0].id, alreadyDone)) {
				cout << "packet of id: " << head.id << "recieved" << endl;
				
				//Find the packet of the right id
				int index = 0;
				while (index < windowSize && packets[index].id != head.id) index++;
				Packet* pack = packets + index;
				
				//If this packet is outside the window parameters, we can't use it.
				if (index == windowSize) {
					if (data != NULL) delete[] data;
					continue;
				}

				//Overwrite the data in the current struct.
				if (pack->content != NULL) delete[] pack->content;
				pack->length = head.length;
				pack->content = data;

				//Set it so that the packet has been received, but not verified
				pack->secured = false;
				pack->transmitted = true;

				//Checksum will be used for confirming integrity of the packet.
				pack->checksum = head.checksum;
			}
			//If we can't use this data, free it.
			else if (data != NULL) {
				delete[] data;
			}
		}
		
		//If we didn't get a single packet this time
		if (!gotPacket) {
			//If this is the 8th time in a row that no data was gotten, give up.
			if (++timesNothingFound == 8) break;
			//Else, give the server time to load.
			else continue;
		}
		
		timesNothingFound = 0;
		sock->signalReady();
		sock->waitReady();

		//For every packet that has not already been confirmed
		for (int i = 0; i < windowSize; i++) {
			Packet* pack = packets + i;
			
			//If the unconfirmed packet matches its checksum, send an ack to the client.
			if (pack->transmitted && !pack->secured && inetChecksum(pack->content, pack->length) == pack->checksum) {
				cout << "Checksum of " << pack->id << " OK"<< endl;
				//Add to successful packet total for stats
				send[2]++;
				sock->sendInt(pack->id);
				cout << "Ack for packet id" << pack->id << "send" << endl;
				
				//Then wait for the other side to be ready for the next int
			}else {
				cout << "Checksum of " << pack->id << " failed" << endl;
			}
		}
		//printing window content
				int i = 0;
				 cout << "Current Window: [";
				for(i = 0; i < windowSize-1; i++){
					cout << packets[i].id << ", " ;
				}
				cout << packets[windowSize -1].id << "]"<< endl;



		// cout << "Waiting for client to send back acks\n";

		while (!sock->waitReady());
		sock->signalReady();

		//cout << "Client ready, waiting for returned acks"<< endl;

		int acked;
		while ((acked = sock->getInt()) != -3) {
			//cout << "Got confirmed ack for " << acked << endl;
			int index = 0;
			while (packets[index].id != acked) index++;
			
			
			//Mark that packet as securely sent
			packets[index].secured = true;
		}

		cout << "\n\n";

		sock->signalReady();
	}

	//Delete the error-tracking array we don't need anymore
	if (alreadyDone != NULL) delete[] alreadyDone;
	
	//Clean up allocated data
	for (int i = 0; i < windowSize; i++) {
		if (packets[i].content != NULL) delete[] packets[i].content;
	}
	
	fclose(file);
	return send;
}


//Uses GO-BACK-N to write socket data to a file
//sock is the read-writer class used to handle socket data
//file is the file in question
//packetSize is the size in bytes of each individual packet.
//windowSize indicates how many packets there are in the window
//sequenceRange is the maximum exclusive bound of the sequence ids (the inclusive min is 0)
//numDropAcks is the length of the list of ids to drop (dropAcks) as part of error simulation.
//Returns an array of longs for use in post-function statistsics
long* GBN(SocketReadWriter* sock, FILE* file, int packetSize, int windowSize, int sequenceRange, int numDropAcks, int* dropAcks){
	long* send = new long[3];
	for (int i = 0; i < 3; i++) {
		send[i] = 0;
	}
	
	//Our window is only 1 packet wide
	Packet packets;
	packets.secured = false;
	packets.id = 0;
	packets.length = packetSize;
	packets.content = new char[packetSize];
	packets.checksum = -1;
	packets.transmitted = packets.terminated = false;

	//Don't want to have unneeded repeats in the error simulation
	bool* alreadyDone = numDropAcks < 1 ? NULL : new bool[numDropAcks];
	for (int i = 0; i < numDropAcks; i++) {
		alreadyDone[i] = false;
	}

	//the linkedlist is a list of packets that will be printed to the file
	//acklist holds the packets that have to be properly added before they can be set to be printed off
	LinkedList *linkedList = new LinkedList(), *ackList = new LinkedList();

	//For use in debugging.
	int count = 1;
	
	
	int timesNothingFound = 0;

	while (true){
		bool first = true;
		//Record the current placement of the window
		int oldStart = packets.id;
		//cout << "Getting header\n";
		
		
		bool gotPacket = false;
		
		//Until the client stops sending packets
		while (sock->getPacket()){
			//Parse the data for the packet
			gotPacket = true;
			Header head;
			char* data = sock->parseData(&head, sequenceRange);
			send[0] = head.id;
			send[1]++;
			
			//Check to see if the data is valid and that we don't feign an error here
			if (data != NULL && !feignError(packets.id, numDropAcks, dropAcks, windowSize, sequenceRange, packets.id, alreadyDone)
				&& (packets.checksum = inetChecksum(data, head.length)) == head.checksum && packets.id == head.id) {
				
				//If the data is valid, add it to the ack list and move the sequence number.
				cout << "Checksum of id " << packets.id << " OK"<< endl;

				send[2]++;

				packets.content = data;
				packets.length = head.length;

				ackList->add(packets);
				//Expect the next sequence number
				packets.id = (packets.id + 1) % sequenceRange;
			}
			//If the data can't be used, say so and free the unusable data.
			else {
				cout << "Checksum of id" << packets.id << " failed"<< endl;
				if (data != NULL) delete[] data;
			}
		}
		
		//If we didn't get a single packet
		if (!gotPacket) {
			//If this is the 8th time in a row that we couldn't get a single packet, call it the end.
			if (++timesNothingFound == 8) break;
			//If there's still a chance the program is loading file data, give it time.
			else continue;
		}
		
		
		timesNothingFound = 0;
		sock->signalReady();
		while (!sock->waitReady());

		// cout << "Client ready, sending ack\n";

		//Cycle through every ackable packet, sending their ids as acks.
		for (int i = 0, max = ackList->getSize(); i < max; i++) {
			Packet pack = ackList->removeFirst();
			sock->sendInt(pack.id);
			cout << "Sending ack of packet id " << pack.id << endl;
			ackList->add(pack);
		}
		cout << "Current Window: [" << packets.id << "]" << endl;

		//cout << "Out of acks, waiting for client to be ready with copies\n";

		while (!sock->waitReady());
		sock->signalReady();
		

		//cout << "Getting copies...\n";

		int ack;
		cout << "Acks returned: ";
		while ((ack = sock->getInt()) != -3) {
			//cout << "oldStart:" << oldStart << " ack:" << ack << endl;
			if (oldStart == ack) {
				oldStart = (oldStart + 1) % sequenceRange;
				Packet pack = ackList->getByID(ack);
				cout << pack.id << ", ";
				linkedList->add(pack);
			}	
		}

		packets.id = oldStart;

		cout << "\n";

		while (ackList->getSize() != 0) {
			ackList->removeFirst();
		}

		cout << "Indicating ready for next loop\n\n";

		sock->signalReady();
    
	}

	if (alreadyDone != NULL) delete[] alreadyDone;

	cout << "Writing listed data to file...\n";

	int i = 1;
	size_t bytes = 0;
	while (linkedList->getSize() != 0){
		packets = linkedList->removeFirst();

		//cout << "REMOVED ONE! " << linkedList->getSize() << " packet(s) to go!\n";

		//cout << "PRINTING PACKET OF ID " << packets.id << " AND CHECKSUM " << packets.checksum << " #" << i++ << endl;
		size_t bytesWritten = 0;
		while (bytesWritten < packets.length) bytesWritten += fwrite(packets.content + bytesWritten, 1, packets.length - bytesWritten, file);
		
		bytes += bytesWritten;
		//cout << "TOTAL BYTES SO FAR: " << bytes << endl;
		delete [] packets.content;
	} 

	fclose(file);
	delete linkedList;
	delete ackList;

	return send;
}
