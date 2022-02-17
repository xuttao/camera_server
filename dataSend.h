/*
 * @Author: xtt
 * @Date: 2021-01-18 12:54:37
 * @Description: ...
 * @LastEditTime: 2022-02-16 16:52:28
 */

#include "NNC_InterfaceControlDocument.h"
#ifdef _UAVLIB
#include "UAV_ImageMOT.h"
#endif
#include "common.h"
#include "packet.h"
#include "socketbase.h"
#include "uvgrtp/lib.hh"
#include <atomic>
#include <functional>
#include <mutex>
class BoxInfo;

namespace std
{
    class thread;
}

class DataSend
{
private:
    DataSend() = default;
    ~DataSend();

private:
    SocketUdp *pSocketUdp = nullptr;
    std::thread *pThread = nullptr;
    std::atomic_bool is_stop = {true};
    std::vector<std::string> current_class_label;
    int refresh = 0; //上报
    uvgrtp::context ctx;
    uvgrtp::session *sess = nullptr;
    uvgrtp::media_stream *sender = nullptr;
    
public:
    int current_modelId = 1;
    int current_streamid = 1;
    static DataSend *getInstance();
    void start();
    inline void set_streamInfo(uint8_t _type)
    {
        if (-1 == type) type = _type;
    }
    void setpf(std::function<void()> _pf) { pf = _pf; }
    struct id_status{
        uint64_t tttime;
        int status;//0 新发现 >0 跟踪中
    };
private:
    bool is_send = false;
    std::function<void()> pf;
    std::atomic<int8_t> type = {-1};

private:
    static void receive_thread(void *);
    void process_model_update(PacketHead *pHead, char *buffer, int len);
    void process_model_check(PacketHead *pHead, char *buffer, int len);
    void process_trace_update(PacketHead *pHead, char *buffer, int len);
    void process_state_check(PacketHead *pHead, char *buffer, int len);
    void process_streamid_update(PacketHead *pHead, char *buffer, int len);

public:
    void send_box(const char **category_list, NNC_InterfaceControlDocument::OutParameterAllInOne *);
#ifdef _UAVLIB
    void send_track_box(const char **category_list, ImageMultiObjTracking::OutParameterAllInOne &data, std::map<int, id_status> &record_global);
#endif
    void send_stream(uint8_t *data, int len);
};
