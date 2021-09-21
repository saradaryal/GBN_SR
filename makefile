#Feel free to change the names of the final executables
CLIENTEXEC = client.exe
SERVEREXEC = server.exe

FLAGS = -D client

all: server.o server.exe client.o client.exe clean

server.o:
	g++ -c main.cpp

server.exe: server.o
	g++ -o $(SERVEREXEC) main.o

client.o: server.exe
	g++ -c main.cpp $(FLAGS)
	
client.exe: client.o
	g++ -o $(CLIENTEXEC) main.o

clean:
	rm main.o
