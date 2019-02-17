#include<iostream>

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include<sys/types.h>
#include <errno.h>

//namespace
using namespace std;

//define
#define STATUS_OK 	  			0
#define MAX_KERNEL_BUFFER_SIZE	(16*1024*1024)

#define MAX_BUFFER_SIZE			256
#define MAX_CONTROL_BUFF_SIZE	2048

#define	PKT_HEADER_SIZE			8
#define UDP_SEND_DATA_SIZE		1024
#define UDP_PAYLOAD_SIZE  		1016

#define END_OF_FILE				-1
#define INVALID_VALUE			-1

#define MAX_FILENAME_SIZE		256
#define MAX_IP_ADDR_SIZE		16
#define MAX_FAIL_CNT_CTL		10000

//class
class UdpFileTransferClient
{
public:
	UdpFileTransferClient(int port,char* addr,char* file);	
	~UdpFileTransferClient();

	void streamFileOverUdp();
	void reSendLostPkt(struct sockaddr_in from, socklen_t fromlen);
	void setupTcpToSendFileName();

private:
	unsigned int port;
	char fileName[MAX_FILENAME_SIZE];
	int sockfd;
	int sockUDPFd;
	struct stat finfo;
	int fd;
	char buffer[MAX_BUFFER_SIZE];	
	char *map;
	int currPktSeq;
	unsigned int numPackets;
	char ipAddress[MAX_IP_ADDR_SIZE];
};

//Globle Variables
UdpFileTransferClient *UdpClient=NULL;

UdpFileTransferClient::UdpFileTransferClient(int portNum,char*ipAddr,char* file)
{
	sockfd 		= INVALID_VALUE;
	fd 		= INVALID_VALUE;
	sockUDPFd 	= INVALID_VALUE;

	map 		= NULL;
	currPktSeq 	= 0;
	numPackets 	= 0;	
	port 		= portNum;	

	strncpy(fileName,file,MAX_FILENAME_SIZE);
	strncpy(ipAddress,ipAddr,MAX_IP_ADDR_SIZE);
}

UdpFileTransferClient::~UdpFileTransferClient()
{
	if(sockfd != INVALID_VALUE)
	{
        shutdown(sockfd, SHUT_RDWR);
		close(sockfd);
		sockfd = INVALID_VALUE;
	}

	if(fd != INVALID_VALUE)
	{
		close(fd);
		fd = INVALID_VALUE;
	}

	if(sockUDPFd != INVALID_VALUE)
	{
        shutdown(sockUDPFd, SHUT_RDWR);
		close(sockUDPFd);
		sockUDPFd = INVALID_VALUE;
	}

	if(map != NULL)
	{
		if(munmap(map, finfo.st_size) == INVALID_VALUE)
		{
			printf("ERROR,line [%d] [%s]\n",__LINE__, strerror(errno));
		}
	}
}

void UdpFileTransferClient::reSendLostPkt (struct sockaddr_in from, socklen_t fromlen) 
{
	int n,i;
	int control[MAX_CONTROL_BUFF_SIZE];
	int failCount=0;

	bzero(buffer,MAX_BUFFER_SIZE);
	cout << "Entering UDP listner" << endl;

	while(1)
	{
#if DEBUG
		cout << "Waiting for UDP NACK..." << endl;
#endif
		bzero(control,MAX_CONTROL_BUFF_SIZE);

		n = recvfrom(sockUDPFd,control,sizeof(control),0,
				(struct sockaddr *)&from,&fromlen);
		if ( n < STATUS_OK)
		{
			cout << "ERROR in recieving NACK";
			usleep(1000);
			++failCount;

			if ( MAX_FAIL_CNT_CTL == failCount)
			{
				cout << "FAILED to Get FILE";
				break;
			}
			else
			{
				continue;
			}
		}
#if DEBUG	
		cout << "NACK: " << control[0] << endl;
#endif
		failCount = STATUS_OK;

		if ( control[0] == END_OF_FILE)
		{
			cout << "File Sent" << endl;
			break;
		}
		else
		{
			char data[UDP_PAYLOAD_SIZE];
			char sendData[UDP_SEND_DATA_SIZE];
			char seqChar[PKT_HEADER_SIZE + 1];

			for ( i=0;i<MAX_CONTROL_BUFF_SIZE;i++)
			{
				if ( control[i] == INVALID_VALUE)
				{
					continue;
				}

				if ( control[i] != numPackets)
				{
					memcpy(data,
							&map[control[i]*UDP_PAYLOAD_SIZE],
							UDP_PAYLOAD_SIZE);

					sprintf(seqChar,"%8d",control[i]);
					memcpy(sendData,seqChar, PKT_HEADER_SIZE);
					memcpy(sendData + PKT_HEADER_SIZE,data,sizeof(data));

					cout <<  "Sending sequence " << control[i] << endl;

					n = sendto(sockUDPFd,sendData,sizeof(sendData),0,
							(struct sockaddr *)&from,fromlen);
					if (n  < STATUS_OK)
					{
						cout << "error sendTo" << endl;
					}
				}
				else
				{
					memcpy(data,
							&map[control[i]*UDP_PAYLOAD_SIZE],
							finfo.st_size - ((numPackets-1)*UDP_PAYLOAD_SIZE));

					sprintf(seqChar,"%8d",control[i]);
					memcpy(sendData,seqChar,PKT_HEADER_SIZE);
					memcpy(sendData + PKT_HEADER_SIZE, data, sizeof(data));

					cout << "Sending sequence " << control[i] << endl;

					n = sendto(sockUDPFd,sendData,sizeof(sendData),0,
							(struct sockaddr *)&from,fromlen);
					if (n  < STATUS_OK)
					{
						cout << "Error sendTo" << endl;
					}
				}
			}
		}
	}

	if(sockUDPFd != INVALID_VALUE)
	{
        shutdown(sockUDPFd, SHUT_RDWR);
		close(sockUDPFd);
		sockUDPFd = INVALID_VALUE;
	}
}

//This Function sends a tcp request to the server 
//to send file name. If Failure in 
//any of the case would terminate client.This change
//could be specific to implementation. 
void UdpFileTransferClient::setupTcpToSendFileName()
{
	int numPktSend = 0;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < STATUS_OK)
	{
		cout << "ERROR opening socket "
				<< strerror(errno) << endl;
		throw;
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ipAddress);
	serv_addr.sin_port = htons(port);

	if (connect(sockfd,(struct sockaddr *) &serv_addr,
			sizeof(serv_addr)) < STATUS_OK)
	{
		cout << "ERROR connecting " << strerror(errno) << endl;
		throw;
	}

	bzero(buffer,MAX_BUFFER_SIZE);

	sprintf(buffer,"%s",fileName);

	numPktSend = write(sockfd,buffer,strlen(buffer));
	if (numPktSend < STATUS_OK)
	{
		cout << "ERROR writing to socket"
				<< strerror(errno) << endl;
		throw;
	}

	bzero(buffer,MAX_BUFFER_SIZE);

	numPktSend = read(sockfd,buffer,(MAX_BUFFER_SIZE - 1));
	if (numPktSend < STATUS_OK)
	{
		cout << "ERROR reading from socket"
				<< strerror(errno) << endl;
		throw;
	}

	cout << "Server Message:" << buffer << endl;

	if (INVALID_VALUE == stat(fileName, &finfo)) 
	{
		cout << "error stating file!!!" << strerror(errno) << endl;
		throw;
	}

	bzero(buffer,MAX_BUFFER_SIZE);

    sprintf(buffer, "%lld", (long long) finfo.st_size);

	//send file details over TCP connection
    cout << "Sending File Size: " << buffer;

	numPktSend = write(sockfd,buffer,strlen(buffer));
	if (numPktSend < STATUS_OK)
	{
		cout << "ERROR writing to socket to send file size" 
				<< strerror(errno) << endl;
		throw;		
	}
}

//send all pkt over udp
void UdpFileTransferClient::streamFileOverUdp()
{
	int numPktSend=0;
	unsigned int length=0;
	struct sockaddr_in server;

	bool isFileSent=false;
	socklen_t fromlen = sizeof(struct sockaddr_in);
	int buffSize = MAX_KERNEL_BUFFER_SIZE;

	cout << "fileName to open" << fileName << endl;

	fd = open(fileName, O_RDONLY);
	if (fd < STATUS_OK)
	{
		cout << "open error" << strerror(errno) << endl;
		throw;
	}

	//open udp connection
    this->sockUDPFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (this->sockUDPFd < STATUS_OK)
	{
		cout << "udp socket open error" << strerror(errno) << endl;
		throw;
	}

	//set input output buffer    
    if (setsockopt(this->sockUDPFd, SOL_SOCKET, SO_SNDBUF,
			&buffSize, sizeof buffSize) < STATUS_OK)
	{
		cout << "buff change error" << strerror(errno) << endl;
	}

    if (setsockopt(this->sockUDPFd, SOL_SOCKET, SO_RCVBUF,
			&buffSize, sizeof buffSize) < STATUS_OK)
	{
		cout << "buff change error" << strerror(errno) << endl;
	}

    server.sin_family 	    = AF_INET;
    server.sin_addr.s_addr  = inet_addr(this->ipAddress);
    server.sin_port 	    = htons(this->port+1);

	length = sizeof(struct sockaddr_in);

	cout << "Starting send....." << endl;

	map = (char*)mmap(0, finfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if ( map == MAP_FAILED)
	{
		if ( fd != INVALID_VALUE)
		{
			close(fd);
			fd = INVALID_VALUE;
		}
		cout << "map" << strerror(errno) << endl;
		throw;
	}

	numPackets = finfo.st_size/UDP_PAYLOAD_SIZE;
	numPackets = numPackets + 1;

	while (1)
	{
		long int size = finfo.st_size;

		for ( long int i=0;i<size;i++)
		{
			char sendData[UDP_SEND_DATA_SIZE];
			char seqChar[PKT_HEADER_SIZE + 1];

            if ( this->currPktSeq == numPackets-1)
			{
				isFileSent = true;
				break;
			}

            if ( this->currPktSeq < numPackets)
			{
				//all packets except the last packet
				sprintf(seqChar,"%8d",currPktSeq);
				memcpy(sendData,seqChar,PKT_HEADER_SIZE);

				memcpy(sendData + PKT_HEADER_SIZE,
						&map[currPktSeq*UDP_PAYLOAD_SIZE],
						UDP_PAYLOAD_SIZE);

				cout << "Sending sequence: " << currPktSeq << endl;

				numPktSend = sendto(sockUDPFd,sendData,sizeof(sendData),
						0,(struct sockaddr *)&server,fromlen);
				if (numPktSend  < STATUS_OK)
				{
					cout << "sendto last seq"
							<< strerror(errno) << endl;
				}
                this->currPktSeq++;
			}
			else
			{ 	//the last packet.
				cout << "Sending the last packet" << endl;

                sprintf(seqChar,"%8d",this->currPktSeq);
				memcpy(sendData,seqChar,PKT_HEADER_SIZE);

				memcpy(sendData + PKT_HEADER_SIZE,
						&map[currPktSeq*UDP_PAYLOAD_SIZE],
						finfo.st_size - (currPktSeq*UDP_PAYLOAD_SIZE));

				cout << "Sending last sequence: " << currPktSeq << endl;

				numPktSend = sendto(sockUDPFd,sendData,sizeof(sendData),
						0,(struct sockaddr *)&server,fromlen);
				if (numPktSend  < STATUS_OK)
				{
					cout << "sendto last seq"
							<< strerror(errno) << endl;
				}
                this->currPktSeq++;
			}
		}

		if ( isFileSent)
		{
            if ( this->sockfd != INVALID_VALUE)
			{
                close(this->sockfd);
                this->sockfd = INVALID_VALUE;
			}
			reSendLostPkt(server, fromlen);
			break;
		}
	}	
}

int main(int argc,char*argv[])
{
	//arg 1 = port number
	//arg 2 = IP address
	//arg 3 = filename
    if(argc != 4)
    {
        cout << "Not Valid Arguments";
    }
    else
	{
		try
		{
			UdpClient = new UdpFileTransferClient(atoi(argv[1]),argv[2],argv[3]);
			if ( UdpClient != NULL)
			{
				UdpClient->setupTcpToSendFileName();
				UdpClient->streamFileOverUdp();

				delete UdpClient;
				UdpClient = NULL;
			}
		}
		catch(...)
		{
			cout << "Exception";
			if ( UdpClient != NULL)
			{		
				delete UdpClient;
				UdpClient = NULL;
			}
		}
	}
	return 0;
}
