#pragma once

// #include "byteArray.h"
#include "log.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
typedef int SOCK_FD;

enum SocketType {
    Server,
    Client,
};

class SocketBase
{
protected:
    SocketType type;
    bool isConnected = false;

public:
    SocketBase() = default;
    virtual ~SocketBase() = default;

public:
    virtual int read(char *pRes, int _size) const = 0;
    //virtual ByteArray read(size_t _readSize) const =0;
    //virtual ByteArray readAll() const=0;
    virtual bool send(const char *_pData, size_t _size) const = 0;
    //virtual bool status() const=0;
    virtual void closeSocket() = 0;
};

class SocketUdp : public SocketBase
{
public:
    SOCK_FD socketfd = -1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

private:
    std::string ip;
    unsigned short port = 0;
    int rcv_size = 0;
    const int waitConMax = 20;

public:
    SocketUdp();
    ~SocketUdp() {}

public:
    bool createUdpServer(const std::string &_ip, unsigned short _port);
    bool createUdpClient(std::string &_serverIp, unsigned short _serverPort, unsigned short _selfPort = 0);
    bool createUdpClient(const char *_serverIp, unsigned short _serverPort, unsigned short _selfPort = 0);
    bool connetToServer(std::string &_serverIp, unsigned short _serverPort) = delete;
    bool send(const char *_pData, size_t _size) const;
    void setRecvBuffer(int _byteSize); //linux下内核会自动翻倍，最小2048，最大为1024*1024
    void setSendBuffer(int _byteSize); //linux下内核自动添加3ms
    // ByteArray read(size_t _readSize) const = delete;
    // ByteArray readAll() const = delete;
    int read(char *pRes, int _size) const;
    void closeSocket();
};

class SocketTcp : public SocketBase
{
public:
    enum STATUS {
        CONNECTED,
        DISCONNECTED,
    };
    SOCK_FD socketfd = -1;
    SOCK_FD socketfd_connect = -1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

private:
    std::string ip;
    unsigned short port = 0;
    int rcv_size = 0;
    const int waitConMax = 20;

public:
    SocketTcp();
    ~SocketTcp();

public:
    void createTcpServer(const std::string &_ip, unsigned short _port);
    void createTcpServer(const char *_ip, unsigned short _port);
    bool connetToServer(std::string &_serverIp, unsigned short _serverPort, int time_out = 10); //超时10秒
    bool connetToServer(const char *_serverIp, unsigned short _serverPort, int time_out = 10);  //超时10秒
    void waitBeConnected(int timeout_sec = 0);                                                  
	SOCK_FD waitBeConnected2();
    bool send(const char *_pData, size_t _size) const;
	bool send(SOCK_FD* fd,const char *_pData, size_t _size) const;
    STATUS getConnectStatus() const;
	STATUS getConnectStatus(SOCK_FD* fd) const;
    void setSendTimeout(int _timeSec, int _timeMsec);
    void setRecvTimeout(int _timeSec, int _timeMsec);
	void setRecvTimeout(SOCK_FD* fd,int _timeSec, int _timeMsec);
    void setRecvBuffer(int _byteSize);
    void setSendBuffer(int _byteSize);
    void setNoDelay();
    // ByteArray read(size_t _readSize) const = delete;
    // ByteArray readAll() const = delete;
    int read(char *pRes, int _size) const;
	int read(SOCK_FD* fd,char *pRes, int _size) const;
    void closeSocket();
	void closeSocket(SOCK_FD* fd);

private:
    inline void setSocketTimeout(int time_sec, int time_usec)
    {
        struct timeval timeout;
        timeout.tv_sec = time_sec;   //秒
        timeout.tv_usec = time_usec; //微秒
        if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
            LOG_ERR("set socket error!");
            FSERVO_CHECK(false);
        }
    }
};