#Feel free to change the names of the final executables
CLIENTEXEC = ottdc6030_aryals9686_client.exe
SERVEREXEC = ottdc6030_aryals9686_server.exe

FLAGS = -D client

all: server.o server.exe client.o client.exe clean

server.o:
	g++ -c ottdc6030_aryals9686_main.cpp

server.exe: server.o
	g++ -o $(SERVEREXEC) ottdc6030_aryals9686_main.o

client.o: server.exe
	g++ -c ottdc6030_aryals9686_main.cpp $(FLAGS)
	
client.exe: client.o
	g++ -o $(CLIENTEXEC) ottdc6030_aryals9686_main.o

clean:
	rm ottdc6030_aryals9686_main.o
