/*
 * @Author: xtt
 * @Date: 2020-12-02 12:00:24
 * @Description: ...
 * @LastEditTime: 2022-02-16 17:42:15
 */
#pragma once
#include "common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#pragma pack(push, 1)

typedef unsigned char uchar;
typedef uint32_t uint;
typedef unsigned long uint64;
typedef unsigned short ushort;

#define PACKET_HEAD_FLAG 0x55
#define PACKET_END_FLAG 0xAA

// #define PACKET_LEN 1024

#define PACKET_HEAD_LEN (sizeof(PacketHead))
#define PACKET_END_LEN (sizeof(PacketEnd))

enum RetType {
    Success = 0x0,
    UnKnowConfig = 0x01, //未知参数
    UnKnowErr = 0xFF,    //未知错误
};

enum PacketType {
    CHECK_CONFIG = 0x01,  //查询参数
    UPDATE_CONFIG = 0x02, //更新参数
    RET,                  //返回值
};

enum PacketSubType {
    STATE = 0x01,    //板卡状态
    TRACE = 0x02,    //跟踪标识
    CLASS = 0x03,    //类别
    WEIGHT = 0x04,   //权重
    STREAMID = 0x05, //视频源ID

    UNKNOW = 0xFF
};

//客户端消息类型
#define PARAM_MODEL_UPDATE 0xc000    //模型加载
#define PARAM_MODEL_CHECK 0xc003     //模型查询
#define PARAM_TRACE_UPDATE 0xc005    //跟踪设置
#define PARAM_STATE_CHECK 0xc009     //状态查询
#define PARAM_STREAMID_UPDATE 0xc101 //视频流id设置
#define PARAM_STREAMID_PUSH 0xc102

//回复类型
#define PARAM_MODEL_UPDATE_ACK 0xc001
#define PARAM_MODEL_UPDATE_RES 0xc002
#define PARAM_MODEL_CHECK_RES 0xc004
#define PARAM_TRACE_UPDATE_ACK 0xc006
#define PARAM_TRACE_UPDATE_RES 0xc007
#define PARAM_TRACE_RES 0xc008
#define PARAM_STATE_RES 0xc010
#define PARAM_STREAMID_RES 0xc102

//视频处理模型加载消息内容
struct Model {
    uchar streamId;
    uchar modelId;
    Model() = default;
    Model(uchar _streamId, uchar _modelId) : streamId(_streamId), modelId(_modelId) {}
};

//
struct ModelInfo {
    Model model;
    uchar classNum;  //类别数
    char *className; //类别名称，每个名称20个字符
    uchar ret;
};

struct TraceInfo {
    uchar streamId;
    uchar traceSw;
    uchar classNum;
};

struct TraceHead {
    uchar streamId;
    uchar traceNum;
};

struct TraceBoxInfo {
    ushort detectId;
    ushort traceId;
    char label[20];
    uchar state=1;
    uint64_t catchtime=0;
    uint64_t disappeartime=0;
    ushort left;
    ushort top;
    ushort width;
    ushort height;
    uchar detectmode;
    uint frameId;

    void transformToBig()
    {
        detectId = htons(detectId);
        traceId = htons(traceId);

		uint32_t high32 = (catchtime & 0xffffffff00000000) >> 32;
    	uint32_t low32 = (catchtime & 0x00000000ffffffff);
    	catchtime = ((uint64_t)(htonl(high32)) | (((uint64_t)(htonl(low32))) << 32));
		
		high32 = (disappeartime & 0xffffffff00000000) >> 32;
    	low32 = (disappeartime & 0x00000000ffffffff);
		disappeartime=((uint64_t)(htonl(high32)) | (((uint64_t)(htonl(low32))) << 32));

        left = htons(left);
        top = htons(top);
        width = htons(width);
        height = htons(height);
        frameId = htonl(frameId);
    }

    void transformToLittle()
    {
        detectId = ntohs(detectId);
        traceId = ntohs(traceId);

		uint32_t high32 = (catchtime & 0xffffffff00000000) >> 32;
    	uint32_t low32 = (catchtime & 0x00000000ffffffff);
    	catchtime = ((uint64_t)(htonl(high32)) | (((uint64_t)(htonl(low32))) << 32));

        //uint64_t disappeartime;

        left = ntohs(left);
        top = ntohs(top);
        width = ntohs(width);
        height = ntohs(height);
        frameId = ntohl(frameId);
    }
};

struct PacketHead {
    ushort msgCode = 0;//2字节 消息代码 
    ushort msgLen = 0;//2字节 消息长度 
    uint timestamp = 0;//4字节 时间戳
    ushort msgSeq = 0;//2字节 消息序列号 
    char msgChannel[9] = {0};//9字节 消息通道 允许为空
    uchar msgPriority = 0; //1字节 消息优先级 
    uchar linkCode = 0;    //1字节 链路代号
    uchar srcNetIden = 0;//1字节 源网络标志 
    ushort srcNetAddr = 0;//2字节 源网络地址 
    uchar dataOutIden = 0;//1字节 数据输出标志 
    uchar dstNetIden = 0;//1字节 目的网络标志 
    ushort dstNetAddr = 0;//2字节 目的网络地址 
    uchar msgType = 0;//1字节 消息类型 
    uchar linkPayload = 0;//1字节 链路负载 
    uchar linkQuality = 0;//1字节 链路质量 
    uchar msgBackIden = 0;//1字节 消息回传标志 （0 无需应答 1 需应答）

    PacketHead() = default;
    PacketHead(ushort _msgcode, ushort _msglen, uchar _msgback) : msgCode(_msgcode), msgLen(_msglen), msgBackIden(_msgback)
    {
    }
    void transformToBig()
    {
        msgCode = htons(msgCode);
        msgLen = htons(msgLen);
        timestamp = htonl(timestamp);
        msgSeq = htons(msgSeq);
        srcNetAddr = htons(srcNetAddr);
        dstNetAddr = htons(dstNetAddr);
    }
    void transformToLittle()
    {
        msgCode = ntohs(msgCode);
        msgLen = ntohs(msgLen);
        timestamp = ntohl(timestamp);
        msgSeq = ntohs(msgSeq);
        srcNetAddr = ntohs(srcNetAddr);
        dstNetAddr = ntohs(dstNetAddr);
    }
};

struct PacketBody {
    char *pData;
};

struct PacketEnd {
    uint64 frameTime;
    uchar endIden; //报文尾标识

    PacketEnd()
    {
        frameTime = get_current_mtime();
        endIden = PACKET_END_FLAG;
    }
};

#pragma pack(pop)