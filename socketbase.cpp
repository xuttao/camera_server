#include "socketbase.h"
#include <errno.h>
#include <netinet/tcp.h>

SocketUdp::SocketUdp()
{
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socketfd == -1) {
        LOG_ERR("%s", "socket init error");
        exit(1);
    }

     struct timeval timeout;
     timeout.tv_sec = 0;     //秒
     timeout.tv_usec = 1000; //微秒
     if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
     {
             LOG_ERR("set socket error!");
     }

    socklen_t optlen = sizeof(int);
    int res = getsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen); //获取发送缓冲区大小
    if (res != -1) {
        LOG_INFO("default rcv_size:%d", rcv_size);
    } else
        LOG_ERR("%s", "get rcv_size error!");
    
    res = getsockopt(socketfd, SOL_SOCKET, SO_SNDBUF, &rcv_size, &optlen); //获取发送缓冲区大小
    if (res != -1) {
        LOG_INFO("default send_size:%d", rcv_size);
    } else
        LOG_ERR("%s", "get send_size error!");
}

bool SocketUdp::createUdpServer(const std::string &_ip, unsigned short _port)
{
    //struct sockaddr_in server_addr;
    type = Server;
    memset(&server_addr, 0, sizeof(sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(_port);

    int ret = bind(socketfd, (sockaddr *)&server_addr, sizeof(sockaddr_in));
    if (ret == -1) {
        LOG_ERR("%s", "bind error!");
        return false;
    }
    return true;
}

bool SocketUdp::createUdpClient(std::string &_serverIp, unsigned short _serverPort, unsigned short _selfPort)
{
    // type = Client;
    // memset(&server_addr, 0, sizeof(sockaddr_in));
    // server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = inet_addr(_serverIp.data());
    // server_addr.sin_port = htons(_serverPort);

    // return true;

    return createUdpClient(_serverIp.c_str(), _serverPort);
}

bool SocketUdp::createUdpClient(const char *_serverIp, unsigned short _serverPort, unsigned short _selfPort)
{
    type = Client;
    memset(&server_addr, 0, sizeof(sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(_serverIp);
    server_addr.sin_port = htons(_serverPort);

    if (_selfPort != 0) {
        memset(&client_addr, 0, sizeof(sockaddr_in));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        client_addr.sin_port = htons(_selfPort);
        int ret = bind(socketfd, (sockaddr *)&client_addr, sizeof(sockaddr_in));
        assert(ret == 0);
    }

    return true;
}

int SocketUdp::read(char *pRes, int _size) const
{
    if (!pRes || _size <= 0) {
        LOG_ERR("%s", "error in read");
        return -1;
    }
    int len = sizeof(sockaddr_in);
    int num = -1;
    if (type == Client) {
        num = recvfrom(socketfd, pRes, _size, 0, (sockaddr *)&server_addr, (socklen_t *)&len);
    } else {
        num = recvfrom(socketfd, pRes, _size, 0, (sockaddr *)&client_addr, (socklen_t *)&len);
    }
    return num;
}

bool SocketUdp::send(const char *_pData, size_t _size) const
{
    size_t ret = 0;
    if (type == Client) {
        ret = sendto(socketfd, _pData, _size, 0, (sockaddr *)&server_addr, sizeof(sockaddr_in));
    } else {
        ret = sendto(socketfd, _pData, _size, 0, (sockaddr *)&client_addr, sizeof(sockaddr_in));
    }
    if (ret != _size) {
        LOG_ERR("send size:%d,", ret);
        return false;
    }
    return true;
}

void SocketUdp::setRecvBuffer(int _byteSize)
{
    socklen_t optlen = sizeof(int);

    int ret = setsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &_byteSize, optlen);
    assert(ret == 0);
}

void SocketUdp::setSendBuffer(int _byteSize)
{
    socklen_t optlen = sizeof(int);

    int ret = setsockopt(socketfd, SOL_SOCKET, SO_SNDBUF, &_byteSize, optlen);
    assert(ret == 0);
}

void SocketUdp::closeSocket()
{
    close(socketfd);
}

/***********************************************************************************************

 *                                                                                      Tcp

***********************************************************************************************/

SocketTcp::SocketTcp()
{
    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd == -1) {
        LOG_ERR("%s", "socket init error");
        exit(1);
    }

    // socklen_t optlen = sizeof(socklen_t);
    // int RecvBuf = 1024 * 1024 * 10;
    //setsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &RecvBuf, sizeof(int));

    socklen_t optlen = sizeof(int);
    int res = getsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen); //获取发送缓冲区大小
    if (res != -1) {
        LOG_INFO("default rcv_size:%d", rcv_size);
    } else
        LOG_ERR("%s", "get rcv_size error!");
}

SocketTcp::~SocketTcp()
{
}

void SocketTcp::createTcpServer(const char *_ip, unsigned short _port)
{
    type = Server;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_aton(_ip, &server_addr.sin_addr);

    int ret = bind(socketfd, (sockaddr *)&server_addr, sizeof(sockaddr_in));
    if (ret < 0) {
        LOG_ERR("bind err:%s", strerror(errno));
        exit(-1);
    }
    ret = listen(socketfd, 1); //only support one connect
    if (ret < 0) {
        LOG_ERR("listen err:%s", strerror(errno));
        exit(-1);
    }
}

void SocketTcp::createTcpServer(const std::string &_ip, unsigned short _port)
{
    return createTcpServer(_ip.c_str(), _port);
}

void SocketTcp::waitBeConnected(int time_out)
{
    // setSocketTimeout(time_out, 0);

    close(socketfd_connect);

    socklen_t len = sizeof(sockaddr_in);
    socketfd_connect = accept(socketfd, (sockaddr *)(&client_addr), &len);

    if (socketfd_connect == -1) {
        LOG_ERR("server wait be connected timeout!");
        // exit(-1);
    }
    LOG_INFO("connected from %s", inet_ntoa(client_addr.sin_addr));

    // setSocketTimeout(0, 3000);
}

SOCK_FD SocketTcp::waitBeConnected2()
{
    socklen_t len = sizeof(sockaddr_in);
    socketfd_connect = accept(socketfd, (sockaddr *)(&client_addr), &len);

    if (socketfd_connect == -1) {
        LOG_ERR("server wait be connected timeout!");
    }
    LOG_INFO("connected from %s", inet_ntoa(client_addr.sin_addr));
    return socketfd_connect;
}

bool SocketTcp::connetToServer(std::string &_serverIp, unsigned short _serverPort, int time_out)
{
    return connetToServer(_serverIp.c_str(), _serverPort, time_out);
}

bool SocketTcp::connetToServer(const char *_serverIp, unsigned short _serverPort, int time_out)
{
    type = Client;
    setSocketTimeout(time_out, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_serverPort);
    inet_aton(_serverIp, &server_addr.sin_addr);

    socklen_t len = sizeof(sockaddr_in);
    int ret = connect(socketfd, (sockaddr *)&server_addr, len);

    if (ret == -1) {
        LOG_ERR("connect to %s failed! port:%u", _serverIp, _serverPort);
        exit(-1);
    }

    LOG_INFO("connect to %s success!", _serverIp);

    setSocketTimeout(0, 0);

    return true;
}

bool SocketTcp::send(const char *_pData, size_t _size) const
{
    size_t ret;
    if (type == Client) {
        ret = write(socketfd, _pData, _size);
    } else {
        ret = write(socketfd_connect, _pData, _size);
    }
    return ret == _size;
}

bool SocketTcp::send(SOCK_FD *fd, const char *_pData, size_t _size) const
{
    size_t ret = write(*fd, _pData, _size);
    return ret == _size;
}

int SocketTcp::read(char *pRes, int _size) const
{
    int ret;
    if (type == Client) {
        ret = recv(socketfd, pRes, _size, 0);
    } else {
        ret = recv(socketfd_connect, pRes, _size, 0);
    }
    return ret;
}

int SocketTcp::read(SOCK_FD *fd, char *pRes, int _size) const
{
    return recv(*fd, pRes, _size, 0);
}

void SocketTcp::setSendTimeout(int _timeSec, int _timeMsec)
{
    int ret;
    struct timeval timeout = {_timeSec, _timeMsec * 1000};
    socklen_t len = sizeof(timeval);
    if (Server == type) {
        ret = setsockopt(socketfd_connect, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
        assert(ret == 0);
        ret = getsockopt(socketfd_connect, SOL_SOCKET, SO_SNDTIMEO, &timeout, &len);
        assert(ret == 0);
    } else {
        ret = setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
        assert(ret == 0);
        ret = getsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, &len);
        assert(ret == 0);
    }
    LOG_INFO("current send time out:%ld", timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
}

void SocketTcp::setRecvTimeout(int _timeSec, int _timeMsec)
{
    int ret;
    struct timeval timeout = {_timeSec, _timeMsec * 1000};
    socklen_t len = sizeof(timeval);
    // socklen_t len2 = sizeof(int);
    if (Server == type) {
        ret = setsockopt(socketfd_connect, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
        assert(ret == 0);
        ret = getsockopt(socketfd_connect, SOL_SOCKET, SO_RCVTIMEO, &timeout, &len);
        assert(ret == 0);
    } else {
        ret = setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
        assert(ret == 0);
        ret = getsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, &len);
        assert(ret == 0);
    }
    LOG_INFO("current recv time out:%ld", timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
}

void SocketTcp::setRecvTimeout(SOCK_FD *fd, int _timeSec, int _timeMsec)
{
    int ret;
    struct timeval timeout = {_timeSec, _timeMsec * 1000};
    socklen_t len = sizeof(timeval);
    ret = setsockopt(*fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
    assert(ret == 0);
    ret = getsockopt(*fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, &len);
    assert(ret == 0);

    LOG_INFO("current recv time out:%ld", timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
}

void SocketTcp::setRecvBuffer(int _byteSize)
{
    int ret;
    socklen_t len = sizeof(int);
    if (Server == type) {
        ret = setsockopt(socketfd_connect, SOL_SOCKET, SO_RCVBUF, &_byteSize, len);
        assert(ret == 0);
        ret = getsockopt(socketfd_connect, SOL_SOCKET, SO_RCVBUF, &_byteSize, &len);
        assert(ret == 0);
    } else {
        ret = setsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &_byteSize, len);
        assert(ret == 0);
        ret = getsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &_byteSize, &len);
        assert(ret == 0);
    }
    LOG_INFO("current recv buffer len:%d", _byteSize);
}

void SocketTcp::setSendBuffer(int _byteSize)
{
    int ret;
    socklen_t len = sizeof(int);
    if (Server == type) {
        ret = setsockopt(socketfd_connect, SOL_SOCKET, SO_SNDBUF, &_byteSize, len);
        if (ret != 0) {
            LOG_ERR("%s", strerror(errno));
            assert(false);
        }

        ret = getsockopt(socketfd_connect, SOL_SOCKET, SO_SNDBUF, &_byteSize, &len);
        assert(ret == 0);
    } else {
        ret = setsockopt(socketfd, SOL_SOCKET, SO_SNDBUF, &_byteSize, len);
        assert(ret == 0);
        ret = getsockopt(socketfd, SOL_SOCKET, SO_SNDBUF, &_byteSize, &len);
        assert(ret == 0);
    }
    LOG_INFO("current send buffer len:%d", _byteSize);
}

void SocketTcp::setNoDelay()
{
    int flag = 1;
    int ret;
    if (Server == type) {
        ret = setsockopt(socketfd_connect, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    } else {
        ret = setsockopt(socketfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    }
    assert(ret == 0);
}

SocketTcp::STATUS SocketTcp::getConnectStatus() const
{
    struct tcp_info info;
    int len = sizeof(info);

    if (type == Client) {
        getsockopt(socketfd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
    } else {
        getsockopt(socketfd_connect, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
    }

    if (TCP_ESTABLISHED == info.tcpi_state) {
        return CONNECTED;
    } else
        return DISCONNECTED;
}

SocketTcp::STATUS SocketTcp::getConnectStatus(SOCK_FD *fd) const
{
    struct tcp_info info;
    int len = sizeof(info);
    getsockopt(*fd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);

    if (TCP_ESTABLISHED == info.tcpi_state) {
        return CONNECTED;
    } else
        return DISCONNECTED;
}

void SocketTcp::closeSocket()
{
    close(socketfd);
    close(socketfd_connect);
}

void closeSocket(SOCK_FD *fd)
{
    close(*fd);
}
