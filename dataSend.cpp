#include "dataSend.h"
#include "bytearray.h"
#include "common.h"
#include "xdmaApi.h"
#include <fstream>
#include <net/if.h>
#include <sys/ioctl.h>
#include <thread>
using namespace NNC_InterfaceControlDocument;

std::atomic_bool can_send={false};
std::atomic_bool is_change={false};
static int get_local_ip(const char *eth_inf, std::string &ip)
{
    int sd;
    struct sockaddr_in sin;
    struct ifreq ifr;

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sd) {
        LOG_ERR("socket error: %s", strerror(errno));
        return -1;
    }

    strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    // if error: No such device
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
        LOG_ERR("ioctl error: %s", strerror(errno));
        close(sd);
        return -1;
    }

    LOG_INFO("interfac: %s, ip: %s", eth_inf, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

    close(sd);
    return 0;
}

static std::vector<std::string> read_class(int id)
{
    const std::string file = std::string("cfg/fpga_class_") + std::to_string(id) + ".txt";
    std::ifstream in(file);
    if (!in.is_open()) {
        LOG_ERR("open file fail:%s", file);
        assert(false);
    }

    std::vector<std::string> vec;
    while (!in.eof() && !in.fail()) {
        std::string lineStr;
        getline(in, lineStr);
        if (lineStr.empty())
            continue;
        vec.push_back(lineStr);
        // LOG_DEBUG("%s", classArr[index]);
    }
    return vec;
}

void DataSend::process_model_update(PacketHead *pHead, char *buffer, int len)
{
    Model *pModel = (Model *)(buffer + PACKET_HEAD_LEN);
    LOG_INFO("receive model update request,streamId:%d,modelId:%d", pModel->streamId, pModel->modelId);

    current_modelId = pModel->modelId;

    // if (pHead->msgBackIden) {
    {
        char temp_buffer[256];
        PacketHead head;
        head = *pHead;
        head.msgCode = PARAM_MODEL_UPDATE_ACK;
        head.msgBackIden = 0;
        head.transformToBig();
        memcpy(temp_buffer, &head, PACKET_HEAD_LEN);
        memcpy(temp_buffer + PACKET_HEAD_LEN, pModel, sizeof(Model));

        pSocketUdp->send(temp_buffer, len);
        LOG_INFO("send model update ack");
    }
    // }
#if 1
    {
        //切换模型参数
        std::string head_file = std::string("cfg/fpga_heads_") + std::to_string(pModel->modelId) + ".bin";
        std::string para_file = std::string("cfg/fpga_paras_") + std::to_string(pModel->modelId) + ".bin";
        LOG_INFO("switch to head:%s,para:%s", head_file.c_str(), para_file.c_str());

        std::string res((para_file + ',' + head_file));
        FpgaParaCfg *pCfg = get_fpga_param();
        pCfg->paramFiles = res;
        update_fpga_param(pCfg);
    }
#endif
    //发送模型加载结果
    auto &&vec_calss = read_class(pModel->modelId);
    current_class_label = vec_calss;
    unsigned short body_len = vec_calss.size() * 20 + sizeof(Model) + 1 + 1; 
    char *send_buffer = new char[PACKET_HEAD_LEN + body_len];

    pHead->msgCode = PARAM_MODEL_UPDATE_RES;
    pHead->msgLen = body_len;
    pHead->msgBackIden = 0;
    pHead->transformToBig();

    int class_size = vec_calss.size();

    memcpy(send_buffer, pHead, PACKET_HEAD_LEN);
    memcpy(send_buffer + PACKET_HEAD_LEN, pModel, sizeof(Model));
    memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(Model), &(class_size), 1);
    for (int n = 0; n < vec_calss.size(); ++n) {
        memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(Model) + 1 + n * 20, vec_calss.at(n).c_str(), vec_calss.at(n).length() + 1);
    }
    uchar ret = 1;
    memcpy(send_buffer + PACKET_HEAD_LEN + body_len - 1, &ret, 1);

    bool res = pSocketUdp->send(send_buffer, PACKET_HEAD_LEN + body_len);
    delete[] send_buffer;
}

void DataSend::process_model_check(PacketHead *pHead, char *buffer, int len)
{
    uchar streamId = *(uchar *)(buffer + PACKET_HEAD_LEN);
    LOG_INFO("receive model check request,streamId:%d", streamId);

    if (current_class_label.empty()) {
        current_class_label = read_class(1);
    }

    unsigned short body_len = current_class_label.size() * 20 + sizeof(Model) + 1;
    char *send_buffer = new char[PACKET_HEAD_LEN + body_len];

    pHead->msgCode = PARAM_MODEL_CHECK_RES;
    pHead->msgLen = body_len;
    pHead->msgBackIden = 0;

    pHead->transformToBig();

    Model model;
    model.streamId = streamId;
    model.modelId = current_modelId;

    int class_size = current_class_label.size();

    memcpy(send_buffer, pHead, PACKET_HEAD_LEN);
    memcpy(send_buffer + PACKET_HEAD_LEN, &model, sizeof(Model));
    memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(Model), &(class_size), 1);
    for (int n = 0; n < current_class_label.size(); ++n) {
        memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(Model) + 1 + n * 20,
               current_class_label.at(n).c_str(), current_class_label.at(n).length() + 1);
    }

    bool res = pSocketUdp->send(send_buffer, PACKET_HEAD_LEN + body_len);
    assert(res);

    delete[] send_buffer;
}

void DataSend::process_trace_update(PacketHead *pHead, char *buffer, int len)
{
    TraceInfo *pTrace = (TraceInfo *)(buffer + PACKET_HEAD_LEN);
    LOG_INFO("receive trace update request,streamId:%d,trace:%d,class num:%d", pTrace->streamId, pTrace->traceSw, pTrace->classNum);

    // if (pHead->msgBackIden) {
    pHead->msgCode = htons((short)PARAM_TRACE_UPDATE_ACK);
    pHead->msgLen = htons(2);
    pHead->msgBackIden = 0;
    char temp_buffer[256];
    uchar res = 1;
    memcpy(temp_buffer, pHead, PACKET_HEAD_LEN);
    memcpy(temp_buffer + PACKET_HEAD_LEN, &(pTrace->streamId), 1);
    memcpy(temp_buffer + PACKET_HEAD_LEN + 1, &res, 1);
    pSocketUdp->send(temp_buffer, PACKET_HEAD_LEN + 2);
    // }

    int class_num_size = pTrace->classNum * 20;
    if(pTrace->classNum!=current_class_label.size()) {is_change = true;}
    
    current_class_label.clear();
    current_class_label.resize(pTrace->classNum);
    for (int n = 0; n < pTrace->classNum; ++n) {
        current_class_label.push_back(std::string(buffer + PACKET_HEAD_LEN + sizeof(TraceInfo) + n * 20, 20));
        LOG_INFO("trace label:%s", (std::string(buffer + PACKET_HEAD_LEN + sizeof(TraceInfo) + n * 20, 20)).c_str());
    }
    /*需要筛选*/

    

    refresh = ntohs(*(ushort *)(buffer + len - 2));//目标消息刷新间隔
    
    LOG_INFO("receive refresh trace time :%dms", refresh);

    {
        int body_size = sizeof(TraceInfo) + class_num_size + 3;
        char *send_buffer = new char[PACKET_HEAD_LEN + body_size];
        pHead->msgCode = PARAM_TRACE_UPDATE_RES;
        pHead->msgBackIden = 0;
        pHead->msgLen = body_size;
        pHead->transformToBig();
        int big_refre = htons(refresh);

        memcpy(send_buffer, pHead, PACKET_HEAD_LEN);
        memcpy(send_buffer + PACKET_HEAD_LEN, pTrace, sizeof(TraceInfo));
        memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(TraceInfo), buffer + PACKET_HEAD_LEN + sizeof(TraceInfo), class_num_size);
        memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(TraceInfo) + class_num_size, &big_refre, 2);
        memcpy(send_buffer + PACKET_HEAD_LEN + body_size - 1, &current_modelId, 1);

        pSocketUdp->send(send_buffer, PACKET_HEAD_LEN + body_size);

        delete[] send_buffer;
    }
}

void DataSend::process_state_check(PacketHead *pHead, char *buffer, int len)
{
    LOG_INFO("receive state check request");

    pHead->msgCode = PARAM_STATE_RES;
    pHead->msgBackIden = 0;
    pHead->msgLen = 1;

    pHead->transformToBig();

    int ret = get_fpga_status() == NNC_CommonStruct::MODEL_STATE_NNC_NORMAL ? 0 : 1;

    char send_buffer[256];
    memcpy(send_buffer, pHead, PACKET_HEAD_LEN);
    memcpy(send_buffer + PACKET_HEAD_LEN, &ret, 1);

    pSocketUdp->send(send_buffer, PACKET_HEAD_LEN + 1);
}

void DataSend::process_streamid_update(PacketHead *pHead, char *buffer, int len)
{
    LOG_INFO("receive streamid update request");

    pHead->msgCode = PARAM_STREAMID_RES;
    pHead->msgBackIden = 0;
    pHead->msgLen = 1;

    //读取当前的视频通道ID TODO current_streamid

    pHead->transformToBig();

    pSocketUdp->send(buffer, len);
}

// void DataSend::send_box(const char **category_list, NNC_InterfaceControlDocument::OutParameterAllInOne *data, std::map<int, id_status> &record_status)
void DataSend::send_box(const char **category_list, NNC_InterfaceControlDocument::OutParameterAllInOne *data)
{

    if(1){
        if(!can_send) return;
        TraceHead body_head;
        body_head.streamId = current_streamid;

        //modify
        body_head.traceNum = data->detectionVec.size();
         /*需要筛选*/
        if(is_change){
            int m_traceNum = 0;
            for(int i=0; i<data->detectionVec.size(); i++){
                std::string category = category_list[data->detectionVec[i].category];
                int ncount = std::count(current_class_label.begin(), current_class_label.end(), category);
                if(ncount > 0) m_traceNum++;
            }
            // body_head.traceNum = data->detectionVec.size();//跟踪目标的信息数量  需要修改为只包含需要跟踪类别的目标信息数量
            body_head.traceNum = m_traceNum;
        }
        

        int body_size = body_head.traceNum * sizeof(TraceBoxInfo);//消息主体长度

        PacketHead head;
        head.msgCode = PARAM_TRACE_RES;
        head.msgLen = sizeof(TraceHead) + body_size;
        head.msgBackIden = 0;
        head.transformToBig();
        int tot_size = PACKET_HEAD_LEN + sizeof(TraceHead) + body_size;
        char *send_buffer = new char[tot_size];

        memcpy(send_buffer, &head, PACKET_HEAD_LEN);
        memcpy(send_buffer + PACKET_HEAD_LEN, &body_head, sizeof(TraceHead));
        for (int n = 0; n < data->detectionVec.size(); ++n) {
            //modify
            std::string category_cur = category_list[data->detectionVec[n].category];
            if(is_change){
                int ncount = std::count(current_class_label.begin(), current_class_label.end(), category_cur);
                if(ncount <= 0) continue;
            }
            
            TraceBoxInfo info;
            info.detectId = n;
            info.traceId = n;

            const char *label = category_cur.c_str();
            memcpy(info.label, label, sizeof(label));

            //根据跟踪结果输出的id维护一个表 更新表 map(id, frame_id)
            /*
            // if(record_status.find(id)!=record_status.end()){ 
                int times = record_status[id].second;
                info.state = times>0?1:0;
                info.catchtime = frameid-times;//得到出现的帧id
                } 
           */
            info.state = 1; //状态 0新发现 1 跟踪中 2 消失
            
            // uint64_t catchtime=0;//1970年到现在的毫秒数 第一次出现的时间
            // uint64_t disappeartime=0;
           
            info.catchtime = 0;//

            info.left = data->detectionVec[n].detectBox.xpointLT;
            info.top = data->detectionVec[n].detectBox.ypointLT;
            info.width = data->detectionVec[n].detectBox.widthBox;
            info.height = data->detectionVec[n].detectBox.heightBox;
            info.detectmode=0;
            info.transformToBig();

            //...
            memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(TraceHead) + n * sizeof(TraceBoxInfo), &info, sizeof(TraceBoxInfo));
        }
       
        pSocketUdp->send(send_buffer, tot_size);
    }
    else{
        if(!can_send) return;
        TraceHead body_head;
        body_head.streamId = current_streamid;
        body_head.traceNum = data->detectionVec.size();

        int body_size = body_head.traceNum * sizeof(TraceBoxInfo);

        PacketHead head;
        head.msgCode = PARAM_TRACE_RES;
        head.msgLen = sizeof(TraceHead) + body_size;
        head.msgBackIden = 0;
        head.transformToBig();
        int tot_size = PACKET_HEAD_LEN + sizeof(TraceHead) + body_size;
        char *send_buffer = new char[tot_size];

        memcpy(send_buffer, &head, PACKET_HEAD_LEN);
        memcpy(send_buffer + PACKET_HEAD_LEN, &body_head, sizeof(TraceHead));
        for (int n = 0; n < data->detectionVec.size(); ++n) {
            //if()
            TraceBoxInfo info;
            info.detectId = 0;
            info.traceId = 0;
            memcpy(info.label, "label", sizeof("label"));
            info.state = 1;
            info.left = data->detectionVec[n].detectBox.xpointLT;
            info.top = data->detectionVec[n].detectBox.ypointLT;
            info.width = data->detectionVec[n].detectBox.widthBox;
            info.height = data->detectionVec[n].detectBox.heightBox;
            info.detectmode=0;
            info.transformToBig();
            //...
            memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(TraceHead) + n * sizeof(TraceBoxInfo), &info, sizeof(TraceBoxInfo));
        }
        pSocketUdp->send(send_buffer, tot_size);
    }
    // LOG_INFO("send box info");
}

#ifdef _UAVLIB
void DataSend::send_track_box(const char **category_list, ImageMultiObjTracking::OutParameterAllInOne &data,  std::map<int, id_status> &record_global)
{

    if(1){
        if(!can_send) return;
        TraceHead body_head;
        body_head.streamId = current_streamid;
        
        std::vector<RESULT_DATA> resVec = data.result;
        //modify
        body_head.traceNum = resVec.size();
         /*需要筛选*/
        if(is_change){
            int m_traceNum = 0;
            for(int i=0; i<body_head.traceNum; i++){
                std::string category = category_list[resVec[i].first.category];
                int ncount = std::count(current_class_label.begin(), current_class_label.end(), category);
                if(ncount > 0) m_traceNum++;
            }
            // body_head.traceNum = data->detectionVec.size();//跟踪目标的信息数量  需要修改为只包含需要跟踪类别的目标信息数量
            body_head.traceNum = m_traceNum;
        }
        
        int body_size = body_head.traceNum * sizeof(TraceBoxInfo);//消息主体长度

        PacketHead head;
        head.msgCode = PARAM_TRACE_RES;
        head.msgLen = sizeof(TraceHead) + body_size;
        head.msgBackIden = 0;
        head.transformToBig();
        int tot_size = PACKET_HEAD_LEN + sizeof(TraceHead) + body_size;
        char *send_buffer = new char[tot_size];

        memcpy(send_buffer, &head, PACKET_HEAD_LEN);
        memcpy(send_buffer + PACKET_HEAD_LEN, &body_head, sizeof(TraceHead));
        for (int n = 0; n < resVec.size(); ++n) {
            //modify
            std::string category_cur = category_list[resVec[n].first.category];
            if(is_change){
                int ncount = std::count(current_class_label.begin(), current_class_label.end(), category_cur);
                if(ncount <= 0) continue;
            }
            
            TraceBoxInfo info;
            info.detectId = n;
            info.traceId = resVec[n].first.id;

            const char *label = category_cur.c_str();
            memcpy(info.label, label, sizeof(label));


            info.state = record_global[info.traceId].status; //状态 0新发现 1 跟踪中 2 消失
            
            // uint64_t catchtime=0;//1970年到现在的毫秒数 第一次出现的时间
            // uint64_t disappeartime=0;
           
            info.catchtime = record_global[info.traceId].tttime;//
            DETECTBOX cur_box =  resVec[n].second;  
            info.left = cur_box(0, 0);
            info.top = cur_box(0, 1);;
            info.width = cur_box(0, 2);;
            info.height = cur_box(0, 3);;
            info.detectmode=0;
            info.transformToBig();

            //...
            memcpy(send_buffer + PACKET_HEAD_LEN + sizeof(TraceHead) + n * sizeof(TraceBoxInfo), &info, sizeof(TraceBoxInfo));
        }
       
        pSocketUdp->send(send_buffer, tot_size);
    }
   
    // LOG_INFO("send box info");
}
#endif

#include <memory>

void DataSend::send_stream(uint8_t *data, int len)
{
#if 1
    uint8_t head_buffer[33];
    
    PacketHead head;
    head.msgCode = PARAM_STREAMID_PUSH; 
    head.msgBackIden = 0;
    auto id = (current_streamid + 10);   
    head.transformToBig();
    memcpy(head_buffer,&head,PACKET_HEAD_LEN);
    memcpy(head_buffer+PACKET_HEAD_LEN,&id,1);
    
    if (sender->push_frame(data, len, head_buffer, 33) != RTP_OK) {
        LOG_ERR("send err");
    }
#else
    bool res = false;
    static unsigned short seq = 0;
    const static int MAX_DATA_LEN = 1472 - PACKET_HEAD_LEN - 1;
    static std::shared_ptr<char> send_buffer(new char[PACKET_HEAD_LEN + 1 + MAX_DATA_LEN], std::default_delete<char[]>());
    char *pbuffer = send_buffer.get();
    PacketHead head;
    head.msgCode = PARAM_STREAMID_PUSH;
    head.msgBackIden = 0;
    auto id = htons(current_streamid + 10);

    if (len > MAX_DATA_LEN) {
        const int send_times = len / MAX_DATA_LEN + (len % MAX_DATA_LEN ? 1 : 0);
        for (int t = 0; t < send_times; ++t) {
            head.msgSeq = seq++;
            if (send_times - 1 == t) {
                head.msgLen = len - (MAX_DATA_LEN * t);
                head.transformToBig();
                memcpy(pbuffer, &head, PACKET_HEAD_LEN);
                memcpy(pbuffer + PACKET_HEAD_LEN, &id, 1);
                memcpy(pbuffer + PACKET_HEAD_LEN + 1, data + (MAX_DATA_LEN * t), len - (MAX_DATA_LEN * t));
                res = pUdp->send(pbuffer, PACKET_HEAD_LEN + 1 + len - (MAX_DATA_LEN * t));
            } else {
                head.msgLen = MAX_DATA_LEN;
                head.transformToBig();
                memcpy(pbuffer, &head, PACKET_HEAD_LEN);
                memcpy(pbuffer + PACKET_HEAD_LEN, &id, 1);
                memcpy(pbuffer + PACKET_HEAD_LEN + 1, data + (MAX_DATA_LEN * t), MAX_DATA_LEN);
                res = pUdp->send(pbuffer, PACKET_HEAD_LEN + 1 + MAX_DATA_LEN);
                // usleep(1000);
            }
        }
    } else {
        head.msgSeq = seq++;
        head.msgLen = len;
        head.transformToBig();
        memcpy(pbuffer, &head, PACKET_HEAD_LEN);
        memcpy(pbuffer + PACKET_HEAD_LEN, &id, 1);
        memcpy(pbuffer + PACKET_HEAD_LEN + 1, data, len);
        res = pUdp->send(pbuffer, PACKET_HEAD_LEN + 1 + len);
    }
    if (!res) LOG_ERR("send stream err len:%s", strerror(errno));
#endif
}

DataSend *DataSend::getInstance()
{
    static DataSend ins;
    return &ins;
}

DataSend::~DataSend()
{
    is_stop = true;
    pThread->join();
    delete pThread;

    if (pSocketUdp) {
        pSocketUdp->closeSocket();
        delete pSocketUdp;
    }
}

void DataSend::start()
{
    std::string local_ip;
// #ifdef _ARM64
//     FSERVO_CHECK(get_local_ip("eth0", local_ip) == 0);
// #else
//     FSERVO_CHECK(get_local_ip("enaftgm1i0", local_ip) == 0);
//     //FSERVO_CHECK(get_local_ip("enp4s0", local_ip) == 0);
// #endif
    local_ip="127.0.0.1";//本地ip
    const char *remote_ip = "127.0.0.1";//目标ip
    const uint16_t local_port = 9300;  
    const uint16_t remote_port = 8990;  

    //视频流推送建立socket
    sess = ctx.create_session(remote_ip);
    assert(sess != nullptr);
    sender = sess->create_stream(local_port, remote_port, RTP_FORMAT_H264, RTP_NO_FLAGS);
    assert(sender != nullptr);

    //控制命令 socket
    pSocketUdp = new SocketUdp;
    pSocketUdp->createUdpServer(local_ip, 16214);

    is_stop = false;
    //开启新线程实时接受控制命令
    pThread = new std::thread(receive_thread, this);
}

void DataSend::receive_thread(void *arg)
{
    DataSend *pThis = static_cast<DataSend *>(arg);
    const int PACKET_LEN = 1500;//一次读的最大长度
    char receive_buffer[PACKET_LEN] = {'0'};
    int index = 0;//未用到
    ByteArray byte_arr;//未用到
    ByteArray full_packet;//未用到
    can_send=false;
    for (;;) {
        if (pThis->is_stop) break;

        int ret = pThis->pSocketUdp->read(receive_buffer, PACKET_LEN);

        if (ret > 0) {
            can_send=true; //只有收到过指令 这边才发送目标信息
            LOG_INFO("receive packet len:%d", ret);
            PacketHead *pHead = (PacketHead *)receive_buffer;
            pHead->transformToLittle();

            ushort type = pHead->msgCode;

            if (PARAM_MODEL_UPDATE == type) {
                pThis->process_model_update(pHead, receive_buffer, ret);
            } else if (PARAM_MODEL_CHECK == type) {
                pThis->process_model_check(pHead, receive_buffer, ret);
            } else if (PARAM_TRACE_UPDATE == type) {
                pThis->process_trace_update(pHead, receive_buffer, ret);
            } else if (PARAM_STATE_CHECK == type) {
                pThis->process_state_check(pHead, receive_buffer, ret);
            } else if (PARAM_STREAMID_UPDATE == type) {
                pThis->process_streamid_update(pHead, receive_buffer, ret);
            }
        } else {
            // LOG_WARN("client disconnected!");
            usleep(10 * 1000);
        }
    }
}
