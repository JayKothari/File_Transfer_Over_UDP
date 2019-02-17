/* Please let me know if any problem in running the program */

*******How to Compile:
1.There are two folder udp_client and udp_server which contains code in it.

2.Inside udp_client also  sample file which  tested for transfer jay.jpg.
  
3.To compile both server and client run:
	make all
for server
	make server
for client
	make client
cleanall
	make clean

*******How to Run:
This program is tested for localhost i.e "127.0.0.1" and local network by sending one file at a time
to server. 

To Run Client:
./client <same as client port number> <client IP address> <file name>
e.g. ./client 8080 127.0.0.1 jay.jpg

To Run Server:
./server <Any Port number>
e.g ./server 8080

*******Description of implementation:

First file_name and file_size would be send to server using tcp connection as we dont want to loose 
the data. 

Then after success of this client would start sending file on udp packets. After sending, udp file
server would send to client packets sequence number that were lost or packets which were not reached to server.

According to the list (control array) send by server, client would start sending packets again.
This process goes on until server does not acknowlege all packet achived by send -1.

******Disclaimer::
Design has been done for simultanous file trasfer but testing has been left.

Do not send simultanous files to trasfer or same file names which is already send or 
file name which are present in the same folder.handling required to be done 

Handling for canceling sending udp packets run time is not handle in server.So dont cancel send
at run time in client application

UDP_RECV_DATA_SIZE macro need to adjust accordng to size of file.Which has to be done dynamic

Thank you!!!!
