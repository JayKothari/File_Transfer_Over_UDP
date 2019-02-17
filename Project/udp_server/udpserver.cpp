//This file is used for Server.cpp

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <libgen.h>

#include <errno.h>
#include <pthread.h>

#define STATUS_OK 					0
#define MAX_CONTROL_BUFF_SIZE		2048

#define MAX_FILENAME_SIZE			256
#define MAX_BUFFER_SIZE				256
#define MAX_KERNEL_BUFFER_SIZE		(16*1024*1024)

#define	PKT_HEADER_SIZE				8
#define UDP_RECV_DATA_SIZE			1024
#define UDP_PAYLOAD_SIZE  			1016

#define MAX_STREAM_FILE				5
#define END_OF_FILE					-1
#define INVALID_VALUE				-1

#define MAX_PKT_SIZE_RECV_BUFF		1000000
#define MAX_TIME_OUT_RECV_UDP		500000000

using namespace std;

typedef enum
{
    FAIL = 0,
    SUCCESS,

    PKT_NOT_RECV=0,
    PKT_RECV
}TYPES_e;

class UdpFileTransfer
{
public:
    int 		numPackets;
    char        *map;

    UdpFileTransfer(int port);
    ~UdpFileTransfer();

    void     openSocketForListening();
    void     setTcpFd(int newFd);
    int      getTcpFd() const;
    int      getPortNo() const;

    long int getFileSize() const;
    int      getOpenFileDes() const;
    int      addrLen() const;
    int      getUdpSockFd() const;

    char*    getFileName();

    bool     getFileInfo();
    bool     openFileToWrite();

    bool     openUdpSocket();

private:
    int         portno;
    int         tcpFd;
    long int    fileSize;
    char        fileName[MAX_FILENAME_SIZE];
    int         fd;
    struct sockaddr_in server;
    int         sockUDP;
};


UdpFileTransfer *udpFileTransfer[MAX_STREAM_FILE];

static void* runNetworkManagerMain(void* arg);

UdpFileTransfer::UdpFileTransfer(int port)
{
    portno 	= port;
    fileSize 	= 0;
    tcpFd 	= INVALID_VALUE;
    fd          = INVALID_VALUE;
    map         = NULL;
    sockUDP     = INVALID_VALUE;
}

UdpFileTransfer::~UdpFileTransfer()
{
    if(tcpFd != INVALID_VALUE)
    {
        shutdown(tcpFd, SHUT_RDWR);
        close(tcpFd);
        tcpFd = INVALID_VALUE;
    }

    if(fd != INVALID_VALUE)
    {
        close(fd);
        fd = INVALID_VALUE;
    }

    if(sockUDP != INVALID_VALUE)
    {
        shutdown(sockUDP, SHUT_RDWR);
        close(sockUDP);
        sockUDP = INVALID_VALUE;
    }
}

void UdpFileTransfer::setTcpFd(int newFd)
{
    tcpFd = newFd;
}

int UdpFileTransfer::addrLen() const
{
    return (sizeof(server));
}

int UdpFileTransfer::getUdpSockFd() const
{
    return sockUDP;
}

long int UdpFileTransfer::getFileSize() const
{
    return fileSize;
}

int UdpFileTransfer::getTcpFd() const
{
    return tcpFd;
}

int UdpFileTransfer::getPortNo() const
{
    return portno;
}

char* UdpFileTransfer::getFileName()
{
    return fileName;
}

int UdpFileTransfer::getOpenFileDes() const
{
    return fd;
}

bool UdpFileTransfer::getFileInfo()
{
    char buffer[MAX_BUFFER_SIZE];
    int  sockTcpfd = this->tcpFd;
    bool ret = true;
    int  recvByte;
    char* bname;

    do
    {
        bzero(buffer,MAX_BUFFER_SIZE);
        recvByte = read(sockTcpfd,buffer,MAX_BUFFER_SIZE -1);
        if (recvByte < STATUS_OK)
        {
            ret = false;
            cout << "Read Error" << strerror(errno) << endl;
            break;
        }

        cout << "File Name:" <<  buffer << endl;

        bname=basename(buffer);
        strcpy(this->fileName,bname);

        bzero(buffer,MAX_BUFFER_SIZE);
        strcpy(buffer,"Received Name from Client");

        recvByte = write(sockTcpfd,buffer,strlen(buffer));
        if(recvByte < STATUS_OK)
        {
            ret = false;
            cout << "ERROR:: write error in tcpsock"
                 << strerror(errno) << endl;
            break;
        }

        bzero(buffer,MAX_BUFFER_SIZE);
        recvByte = read(sockTcpfd,buffer,MAX_BUFFER_SIZE - 1);
        if(recvByte < STATUS_OK)
        {
            ret = false;
            cout << "ERROR:: read file size error"
                 << strerror(errno) << endl;
        }

        cout << "File Size: " << buffer << endl;

        this->fileSize = atoi(buffer);
        if(this->fileSize <= STATUS_OK)
        {
            ret = false;
            break;
        }
    }while(0);

    return ret;
}

bool UdpFileTransfer::openFileToWrite()
{
    bool ret = true;
    int result=0;

    do
    {
        this->fd = open(this->getFileName(),
                        O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
        if (this->fd < STATUS_OK)
        {
            ret = false;
            cout << "Error:: open error" << strerror(errno) << endl;
            break;
        }

        result = lseek(this->fd, (this->getFileSize())-1, SEEK_SET);
        if (result < STATUS_OK)
        {
            if(this->fd != INVALID_VALUE)
            {
                shutdown(this->fd, SHUT_RDWR);
                close(this->fd);
                this->fd = INVALID_VALUE;
            }
            cout << "Error:: seek error" << strerror(errno) << endl;
            ret = false;
            break;
        }

        result = write(fd, "", 1);
        if (result != 1)
        {
            if(this->fd != INVALID_VALUE)
            {
                shutdown(this->fd, SHUT_RDWR);
                close(this->fd);
                this->fd = INVALID_VALUE;
            }
            cout << "Error:: in write" << strerror(errno) << endl;
            ret = false;
            break;
        }

        this->map = (char*)mmap(0, this->getFileSize(),
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                this->fd, 0);
        if (MAP_FAILED == this->map)
        {
            if(this->fd != INVALID_VALUE)
            {
                shutdown(this->fd, SHUT_RDWR);
                close(this->fd);
                this->fd = INVALID_VALUE;
            }
            cout << "Error:: in maping" << strerror(errno) << endl;
            ret = false;
            break;
        }
    }while(0);

    return ret;
}

bool UdpFileTransfer::openUdpSocket()
{
    bool ret = true;
    int  length = 0;

    do
    {
        //udp setup
        this->sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockUDP < STATUS_OK)
        {
            cout << "ERROR:: Error in UDP socket"
                 << strerror(errno) << endl;
            ret = false;
            break;
        }

        int buffSize = MAX_KERNEL_BUFFER_SIZE;
        if(setsockopt(sockUDP, SOL_SOCKET, SO_SNDBUF,
                      &buffSize, sizeof buffSize) < STATUS_OK)
        {
            cout << "ERROR:: Error in set kernel send buffer"
                 << strerror(errno) << endl;
            //ret = false;
            //break;
        }

        if(setsockopt(sockUDP, SOL_SOCKET, SO_RCVBUF,
                      &buffSize, sizeof buffSize) < STATUS_OK)
        {
            cout << "ERROR:: Error in set kernel recv buffer"
                 << strerror(errno) << endl;
            //ret = false;
            //break;
        }

        length = sizeof(this->server);
        bzero(&this->server,length);

        this->server.sin_family=AF_INET;
        this->server.sin_addr.s_addr=INADDR_ANY;
        this->server.sin_port=htons(this->getPortNo() + 1);

        if (bind(this->sockUDP,
                 (struct sockaddr *)&this->server,
                 length) < STATUS_OK)
        {
            cout << "ERROR:: bind Error "
                 << strerror(errno) << endl;
            ret = false;
            break;
        }

    }while(0);

    return ret;
}

void* runNetworkManagerMain(void* arg)
{
    UdpFileTransfer* udpFileTx = (UdpFileTransfer*)arg;

    int n=0;

    int sockUDP = INVALID_VALUE, length=0;
    socklen_t fromlen;
    //	struct sockaddr_in server;
    struct sockaddr_in from;

    int sockTcpfd = udpFileTx->getTcpFd();

    do
    {
        if(INVALID_VALUE == sockTcpfd)
        {
            cout << "error::sockTcpfd" << endl;
            break;
        }

        if(!udpFileTx->getFileInfo())
        {
            cout << "error::getFileInfo" << endl;
            break;
        }

        if(!udpFileTx->openFileToWrite())
        {
            cout << "error::openFileToWrite" << endl;
            break;
        }

        char recvData[UDP_RECV_DATA_SIZE];
        char data[UDP_RECV_DATA_SIZE - PKT_HEADER_SIZE];
        long int recvSize=0;
        long int totPkts=(udpFileTx->getFileSize()/(UDP_RECV_DATA_SIZE - PKT_HEADER_SIZE));
        totPkts = totPkts + 1;
        char pktsRcvd[MAX_PKT_SIZE_RECV_BUFF]={};
        int nackCounter=0;

        if(!udpFileTx->openUdpSocket())
        {
            cout << "error::openUdpSocket()" << endl;
            break;
        }

        sockUDP = udpFileTx->getUdpSockFd();

        fromlen = sizeof(struct sockaddr_in);

        //parameters for select
        struct timespec tv;
        fd_set readfds;

        //set the timeout values
        tv.tv_sec = 0;
        tv.tv_nsec = MAX_TIME_OUT_RECV_UDP;

        //sockfd has to be monitored for timeout
        FD_ZERO(&readfds);
        FD_SET(sockUDP, &readfds);

        while(1)
        {
            //wait fro recvfrom or timeout
            FD_ZERO(&readfds);
            FD_SET(sockUDP, &readfds);

            int setNumFd = pselect(sockUDP+1, &readfds, NULL, NULL, &tv, NULL);

            if(setNumFd > 0 )
            {
                n = recvfrom(sockUDP,recvData,UDP_RECV_DATA_SIZE,0,
                             (struct sockaddr *)&from, (socklen_t*)&length);

                if(n < STATUS_OK)
                {
                    cout << "ERROR :: recv error"
                         << strerror(errno) << endl;
                    continue;
                }

                char seqC[PKT_HEADER_SIZE];
                memcpy(seqC,recvData,PKT_HEADER_SIZE);
                int seq = atoi(seqC);

                if(PKT_NOT_RECV == pktsRcvd[seq])
                {
                    cout << "Sequence Received " << udpFileTx->numPackets << seq << endl;
                    udpFileTx->numPackets++;

                    pktsRcvd[seq] = PKT_RECV;

                    memcpy(data,
                           recvData + PKT_HEADER_SIZE,
                           sizeof(recvData)- PKT_HEADER_SIZE);

                    if(seq < totPkts-1)
                    {
                        memcpy(&udpFileTx->map[seq*(UDP_RECV_DATA_SIZE - PKT_HEADER_SIZE)],
                               data,sizeof(data));

                        recvSize += sizeof(data);
                    }
                    else
                    {
                        cout << "Writing last packet" << endl;
                        memcpy(&udpFileTx->map[seq*(UDP_RECV_DATA_SIZE - PKT_HEADER_SIZE)],
                               data,
                               (udpFileTx->getFileSize()) - ((seq)*(UDP_RECV_DATA_SIZE - PKT_HEADER_SIZE)));

                        recvSize += ((udpFileTx->getFileSize()) - ((seq)*(UDP_RECV_DATA_SIZE - PKT_HEADER_SIZE)));
                    }

                    if(recvSize >= (udpFileTx->getFileSize()))
                    {
                        cout << "File received\n" << endl;
                        cout << "Filesize: " << udpFileTx->getFileSize() << endl;
                        cout << "Recv size: " <<  recvSize << endl;
#if DEBUG
                        cout << "Total Nack packets: " << nackCounter << endl;
#endif

                        cout << "Total Nack packets: " << nackCounter << endl;
                        if (munmap(udpFileTx->map, udpFileTx->getFileSize()) == -1)
                        {
                            //cout << "ERROR,line [%d] [%s]\n",__LINE__, strerror(errno));
                        }

                        udpFileTx->map = NULL;

                        if(udpFileTx->getOpenFileDes() != INVALID_VALUE)
                        {
                            close(udpFileTx->getOpenFileDes());
                        }

                        int final[1];
                        final[0] = END_OF_FILE;

                        //packet should not get missed
                        for(int repCnt=0;repCnt<100;++repCnt)
                        {
                            n = sendto(sockUDP,final,sizeof(final),0,
                                       (struct sockaddr *)&from, length);
                            if (n < STATUS_OK)
                            {
                                cout << "ERROR:: Send UDP Pkt" << strerror(errno) << endl;
                            }
                        }
                        break;
                    }
                }
            }
            else
            {
                cout << "Number of bytes rxed: " << recvSize;
#if DEBUG
                cout << "Entering timeout loop";
#endif
                int control[MAX_CONTROL_BUFF_SIZE];
                long int reqs=0;
                int pktCnt;
                int num=0;

                bzero(control,MAX_CONTROL_BUFF_SIZE);
                for(pktCnt=0;pktCnt<totPkts;pktCnt++)
                {
                    if(PKT_NOT_RECV == pktsRcvd[pktCnt])
                    {
                        control[num] = pktCnt;
                        num++;

                        reqs++;
                        if(reqs == MAX_CONTROL_BUFF_SIZE)
                        {
                            nackCounter++;
                            num=0;

                            for(int repCnt=0;repCnt<2;++repCnt)
                            {
                                n = sendto(sockUDP,control,sizeof(control),0,
                                           (struct sockaddr *)&from, length);
                                if (n < STATUS_OK)
                                {
                                    cout << "ERROR:: "
                                         <<  strerror(errno) << endl;
                                }
                            }
                            break;
                        }
                    }
                }

                //all packet received
                if(pktCnt == totPkts)
                {
                    nackCounter++;

                    for(int repCnt=0;repCnt<2;++repCnt)
                    {
                        n = sendto(sockUDP,control,sizeof(control),
                                   0,(struct sockaddr *)&from, length);
                        if (n < 0)
                        {
                            cout << "ERROR :: "
                                 << strerror(errno) << endl;
                        }
                    }
                }

                // printf("Number of bytes rxed: %ld\n",recvSize);
                FD_ZERO(&readfds);
                FD_SET(sockUDP, &readfds);
            }
        }
    }while(0);

    //udp
    if(udpFileTx != NULL)
    {
        delete udpFileTx;
        udpFileTx = NULL;
    }

    cout << "Thread Exit"<< endl;

    pthread_exit(NULL);
}

int main(int argc,char *argv[])
{
    int tcpSocket=INVALID_VALUE, newsockfd, pid;
    socklen_t clilen;
    struct sockaddr_in servAddr, cliAddr;
   // int portno;
    char startNewThread = SUCCESS;

    pthread_attr_t		attr;
    pthread_t		threadId;

    if (argc < 2)
    {
        cout << "ERROR,line "<< endl;
        exit(1);
    }

    for(int cnt = 0;cnt<MAX_STREAM_FILE;++cnt)
    {
        udpFileTransfer[cnt] = NULL;
    }

    //TCP setup
    do
    {
        tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcpSocket < STATUS_OK)
        {
            cout << "socket ERROR:: ::::" << endl;
            startNewThread = FAIL;
            break;
        }

        bzero((char *) &servAddr, sizeof(servAddr));

        servAddr.sin_family = AF_INET;
        servAddr.sin_addr.s_addr = INADDR_ANY;
        servAddr.sin_port = htons(atoi(argv[1]));

        if(bind(tcpSocket, (struct sockaddr *) &servAddr,
                sizeof(servAddr)) < STATUS_OK)
        {
            cout << "bind ERROR:: "<< endl;
            startNewThread = FAIL;
            break;
        }

        if(listen(tcpSocket,MAX_STREAM_FILE) < STATUS_OK)
        {
            cout << "listen ERROR::"<< endl;
            startNewThread = FAIL;
            break;
        }

    }while(0);

    clilen = sizeof(cliAddr);

    if(SUCCESS == startNewThread)
    {
        while (1)
        {
            cout << "Waiting for connection" << endl;
            newsockfd = accept(tcpSocket,
                               (struct sockaddr *) &cliAddr, &clilen);

            if (newsockfd < STATUS_OK)
            {
                cout << "ERROR::" << strerror(errno) << endl;
            }

            int cnt = 0;
            for(;cnt<MAX_STREAM_FILE;++cnt)
            {
                if(NULL == udpFileTransfer[cnt])
                {
                    break;
                }
            }

            udpFileTransfer[cnt] = new UdpFileTransfer(atoi(argv[1]));
            if(udpFileTransfer[cnt] == NULL)
            {
                break;
            }

            pthread_attr_init(&attr);			//initialize thread attribute
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

            udpFileTransfer[cnt]->setTcpFd(newsockfd);

            if(pthread_create(&threadId,&attr,
                              &runNetworkManagerMain,udpFileTransfer[cnt]) != STATUS_OK)
            {
                close(newsockfd);
                break;
            }
        } /* end of while */
    }

    close(tcpSocket);
    pthread_attr_destroy(&attr);

    return 0;
}
