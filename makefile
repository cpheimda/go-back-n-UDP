server: server.c files.c pgmread.c
	gcc -g -Wall -c server.c files.c pgmread.c
	gcc -o server server.o files.o pgmread.o

client: client.c files.c send_packet.c linkedlist.c
	gcc -g -Wall -c client.c files.c send_packet.c linkedlist.c
	gcc -o client client.o files.o send_packet.o linkedlist.o

crun: client
	./client 0.0.0.0 9002 list_of_filenames.txt 10