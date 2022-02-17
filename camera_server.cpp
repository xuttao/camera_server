#include "camera_server.h"
#include "NNC_InterfaceControlDocument.h"
#include "h264writer.h"
#include "mempool.hpp"
#include "semaphore.h"
#include "socketbase.h"
#include "xdmaApi.h"

#include <sys/time.h>
#include <unistd.h>

#include "dataSend.h"
#include <atomic>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <thread>

#define MAX_PATH 128
#ifdef _UAVLIB
#include "UAV_ImageMOT.h"
#endif

using namespace NNC_InterfaceControlDocument;

namespace
{
    int camera_width = 0;
    int camera_height = 0;
    std::Semaphore sem_post;
    std::Semaphore sem_mat;
    std::Semaphore sem_post_start;
    std::Semaphore sem_write;
    std::Semaphore sem_write_start;
    struct CallBackModel {
        FpgaInput *pInput = nullptr;
        BoxInfo *pBox = nullptr;
        int boxNum = 0;
        CallBackModel(FpgaInput *pIn, BoxInfo *pB, int n) : boxNum(n), pInput(pIn)
        {
            if (n > 0) {
                pBox = new BoxInfo[n];
                memcpy(pBox, pB, sizeof(BoxInfo) * n);
            }
        }

        CallBackModel() = default;
    };

    std::mutex mutex_show;
#ifdef _DOUBLE_V7
    std::queue<cv::Mat> que_show_mat;
#else
    std::queue<CallBackModel> que_show;
#endif
    std::queue<cv::Mat> que_mat;
    std::mutex mutex_mat;
    /*2021 11 19 by zm*/
    std::queue<AVPacket> que_pkt;
    std::mutex mutex_pkt;

    std::atomic_bool is_stop = {true};
    std::thread *thread_receive = nullptr;//读视频源数据线程
    std::thread *thread_input = nullptr; //将视频帧数据传入给FPGA线程
    std::thread *thread_display = nullptr; //显示线程

    std::thread *thread_post = nullptr;//推流线程

    //类别数组 应该从本地的build/cfg/fpga_class文件中读 //TODO 
    const char *names_fptr[] = {"person", "car", "bus", "motorcycle", "airplane", "bus", "train", "truck", "boat",
                                "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
                                "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
                                "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
                                "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
                                "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
                                "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
                                "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote",
                                "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book",
                                "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};

    const int color_num = 18;

    const char *pColor[color_num] = {"220,20,60", "0,0,255", "30,144,255", "0,255,255", "0,255,0", "255,215,0", "255,165,0", "210,105,30", "255,69,0", "	255,0,0",
                                     "148,0,211", "70,130,180", "32,178,170", "85,107,47", "255,215,0", "255,140,0", "255,127,80", "0,191,255"};

    std::map<int, std::vector<unsigned char>> mp_color;
    uint64_t picNum = 0;

    bool is_opend = false;

    slpool::MemPool<InParameterAllInOne> input_pool;

#ifdef _UAVLIB
    ImageMultiObjTracking::UAV_ImageMOT* pUavImageMot = new ImageMultiObjTracking::UAV_ImageMOT();
    ImageMultiObjTracking::InParameterAllInOne MOTinParaAllInOne;
    ImageMultiObjTracking::OutParameterAllInOne MOToutParaAllInOne;	
#endif
    // struct id_status{
    //     uint64_t tttime;
    //     int status;//0 新发现 >0 跟踪中
    // };
   
    std::map<int, DataSend::id_status> record_global;

} // namespace

#define FPGA

CameraServer::CameraServer()
{
}

CameraServer::~CameraServer()
{
    fpga_stop();
    fpga_close();
}

static void draw_box(cv::Mat &imgdata, const std::vector<NNC_CommonStruct::NNC_Detection> &vecbox)
{
    for (uint num = 0; num < vecbox.size(); ++num) {
        auto &pBoxData = vecbox[num];

        int x1 = pBoxData.detectBox.xpointLT;
        int y1 = pBoxData.detectBox.ypointLT;

        float score = pBoxData.confidence;
        int class_no = pBoxData.category;

        cv::Rect r(x1, y1, pBoxData.detectBox.widthBox, pBoxData.detectBox.heightBox);

        cv::Scalar cvColor;
        auto ite = mp_color.find(class_no);
        if (ite == mp_color.end()) {
            const char *color = pColor[num % color_num];
            auto vec = split_string(std::string(color), ',');

            std::vector<unsigned char> vec2;
            vec2.resize(3);
            vec2[0] = std::atoi(vec[0].c_str());
            vec2[1] = std::atoi(vec[1].c_str());
            vec2[2] = std::atoi(vec[2].c_str());

            mp_color[class_no] = vec2;
            cvColor = cv::Scalar(vec2[0], vec2[1], vec2[2]);
        } else {
            cvColor = cv::Scalar(ite->second[0], ite->second[1], ite->second[2]);
        }

        cv::rectangle(imgdata, r, cvColor, 2, 1, 0);
        // std::cout << "---------------draw box :" << x1 << " " << y1 << " " << x2 - x1 << " " << y2 - y1 << " " << std::endl;

        int fontFace = 0;
        double fontScale = 0.4;
        int thickness = 1;
        int baseline = 0;

        cv::Size textSize = cv::getTextSize(std::string(names_fptr[class_no]) + std::to_string(score), fontFace, fontScale, thickness, &baseline);
        cv::rectangle(imgdata, cv::Point(x1 - 1, y1), cv::Point(x1 + textSize.width + thickness, y1 - textSize.height - 4), cvColor, -1, 1, 0);
        cv::Point txt_org(x1 - 1, y1 - 4);
        cv::putText(imgdata, std::string(names_fptr[class_no]) + " " + std::to_string(score), txt_org, fontFace, fontScale, cv::Scalar(255, 255, 255));
    }
}

#ifdef _UAVLIB
static void call_back(OutParameterAllInOne *output, InParameterAllInOne *input, void *arg)
{
    NNC_CommonStruct::NNC_Timer timer1 = output->nncTimer;
   
    // time_t tt = timer1.absTime/1000;
    //track input
    MOTinParaAllInOne.realtimeImage = output->currentFrame;
    
    DETECTIONS detections;
    for(int i=0; i< output->detectionVec.size(); i++){
        NNC_CommonStruct::NNC_Detection cur_detection= output->detectionVec[i];
        DETECTION_ROW tmpRow;
        tmpRow.tlwh = DETECTBOX(cur_detection.detectBox.xpointLT, cur_detection.detectBox.ypointLT, cur_detection.detectBox.widthBox, cur_detection.detectBox.heightBox);
        tmpRow.confidence = cur_detection.confidence;
        tmpRow.confidence = 1.0f;
        tmpRow.category = cur_detection.category; 

        detections.push_back(tmpRow);
    }
    MOTinParaAllInOne.detections = detections;
    //track
    MOToutParaAllInOne = pUavImageMot->MotUpdate(MOTinParaAllInOne, names_fptr);
         
    cv::Mat res;       
    MOToutParaAllInOne.realtimeImageWithLable.copyTo(res);
    mutex_show.lock();
    que_show_mat.push(res);
    mutex_show.unlock();
    input_pool.delElement(input);
    sem_post.post();
    
    // DataSend::getInstance()->send_box(output);
    std::vector<RESULT_DATA> vecRes = MOToutParaAllInOne.result;
    if(record_global.empty()){
        for(int i=0; i<vecRes.size();i++){
            DataSend::id_status cur_status={timer1.absTime, 0};
            record_global[vecRes[i].first.id] = cur_status;
        }
    }else{
        std::map<int, DataSend::id_status> record_global_tmp;
        for(int i=0; i<vecRes.size();i++){
            int cur_id = vecRes[i].first.id;
            if(record_global.find(cur_id)!=record_global.end()){
                DataSend::id_status cur_status={record_global[cur_id].tttime, 1};//跟踪中
                record_global_tmp[vecRes[i].first.id] = cur_status;
            }else{
                DataSend::id_status cur_status={timer1.absTime, 0};//新发现
                record_global_tmp[vecRes[i].first.id] = cur_status;
            }
        }
        record_global = record_global_tmp;
    }

    static camera_demo *pThis = static_cast<camera_demo *>(arg);
    static auto t1 = QDateTime::currentMSecsSinceEpoch();
    picNum++;
    if (picNum % 25 == 0 && picNum != 25) {
        auto t2 = QDateTime::currentMSecsSinceEpoch();
        auto fps = 25.f / (t2 - t1) * 1000;
        std::cout << "--------------fps:" << 25.f / (t2 - t1) * 1000 << std::endl;
        t1 = t2;
        fflush(stdout);

        pThis->ui.lbeFps->setText(QLatin1String("fps:") + QString::number(fps, 'f', 2));
        // DataSend::getInstance()->send_box(names_fptr, output);//每个大概一秒钟发送目标信息
        DataSend::getInstance()->send_track_box(names_fptr, MOToutParaAllInOne, record_global);
        // void DataSend::send_box(const char **category_list, NNC_InterfaceControlDocument::OutParameterAllInOne *data, std::map<int, id_status> &record_global)
    }
}
#else
static void call_back(OutParameterAllInOne *output, InParameterAllInOne *input, void *arg)
{
    draw_box(output->currentFrame, output->detectionVec);

        
    cv::Mat res;
    output->currentFrame.copyTo(res);
    mutex_show.lock();
    que_show_mat.push(res);
    mutex_show.unlock();
    input_pool.delElement(input);
    sem_post.post();

    // DataSend::getInstance()->send_box(output);

    static CameraServer *pThis = static_cast<CameraServer *>(arg);
    static auto t1 = get_current_mtime();
    picNum++;
    if (picNum % 25 == 0 && picNum != 25) {
        auto t2 = get_current_mtime();
        auto fps = 25.f / (t2 - t1) * 1000;
        std::cout << "--------------fps:" << 25.f / (t2 - t1) * 1000 << std::endl;
        t1 = t2;
        fflush(stdout);
        DataSend::getInstance()->send_box(names_fptr,output);
    }
}
#endif

void CameraServer::run()
{
	slot_btn();
	thread_receive->join();
}

//采集按钮按下
void CameraServer::slot_btn()
{
    if (!is_play) {
#ifdef FPGA
        // if (!is_opend)
        // {
        if (!fpga_open()) {
            fprintf(stderr, "xdma open failed\n");
            return;
        }
        set_callback(call_back, (void *)this);
        fpga_start();
        is_opend = true;
        // }
#endif
#ifdef _UAVLIB
	    pUavImageMot->InitImgMultiObjTracker(MOTinParaAllInOne);
#endif
        camera_width = camera_height = 0;
        picNum = 0;
        while (!que_mat.empty())
            que_mat.pop();
        // while (!que_show.empty())
        //     que_show.pop();

        is_stop = false;

        sem_mat.init(0);
        sem_post.init(0);

        DataSend::getInstance()->start();

        thread_receive = new std::thread(receive_thread, this);
        thread_input = new std::thread(input_thread, this);

        is_play = true;
        // thread_display = new std::thread(save_thread, this);
        thread_post = new std::thread(post_thread, this);

    } else {
        is_stop = true;

        thread_receive->join();
        thread_input->join();
        thread_post->join();
        thread_display->join();

        delete thread_receive;
        delete thread_input;
        delete thread_post;
        delete thread_display;
        //        timer.stop();

        fpga_stop();
        fpga_close();

        is_play = false;
    }
}

void CameraServer::input_thread(void *arg)//给FPGA喂入数据
{
    uint64_t index = 0;
    for (;;) {
        sem_mat.wait();

        if (is_stop) break;

        mutex_mat.lock();
        cv::Mat frame = que_mat.front();
        if (que_mat.size() >= 2) {
            //帧率不足时丢帧处理
        }
        que_mat.pop();
        mutex_mat.unlock();
#ifdef _DOUBLE_V7
        InParameterAllInOne *pInput = input_pool.newElement();
        pInput->idStreamVideo = 0;
        pInput->currentFrame = frame;

        // pInput->nncTimer = {QDateTime::currentDateTimeUtc().date().year(),
        //                     QDateTime::currentDateTimeUtc().date().month(), QDateTime::currentDateTimeUtc().date().day(),
        //                     QDateTime::currentMSecsSinceEpoch(), QDateTime::currentMSecsSinceEpoch()};
        pInput->nncStateModel = get_fpga_status();

#else
        FpgaInput *pInput = new FpgaInput;
        pic_process(pInput, frame);
        int rgb_len = frame.rows * frame.cols * frame.channels() * frame.elemSize1();
        pInput->src_data = new uint8_t[rgb_len];
        memcpy(pInput->src_data, frame.data, rgb_len);
        pInput->height = frame.rows;
        pInput->width = frame.cols;
        pInput->name = std::move(std::to_string(index++));
#endif
        push_data((void *)pInput);
        frame.release();
    }
}

//读取视频源数据
void CameraServer::receive_thread(void *arg)
{
    
    cv::VideoCapture rtspCap;

    std::ifstream in("cfg/url.txt");
    if (!in.is_open()) {
        fprintf(stderr, "not found url.txt in cfg");
        assert(false);
    }

    std::string strUrl;
    std::getline(in, strUrl);
    if (strUrl.empty()) {
        fprintf(stderr, "url is empty");
        assert(false);
    }
    std::cout << "OpenCV version : " << CV_VERSION << std::endl;

    bool res = rtspCap.open(strUrl.c_str());
    if (!res) {
        fprintf(stderr, "url open failed\n");
        assert(false);
    }

    for (;;) {
        if (is_stop) break;

        if (!rtspCap.isOpened()) {
            fprintf(stderr, "cap close\n");
            continue;
        }
        cv::Mat frameTemp;
        rtspCap >> frameTemp;

        if (frameTemp.empty()) {
            fprintf(stderr, "empty frame\n");
            continue;
        } else if (!camera_height) {
            camera_width = frameTemp.cols;
            camera_height = frameTemp.rows;
            sem_post_start.post();
            sem_write_start.post();
        }
        cv::Mat frame;
        cv::cvtColor(frameTemp, frame, cv::COLOR_BGR2RGB);
#ifdef FPGA
        mutex_mat.lock();
        que_mat.push(frame);
        // que_mat.push(frameTemp);
        mutex_mat.unlock();
        sem_mat.post();
#else
        FpgaInput *pInput = new FpgaInput;
        pInput->data = new uint8_t[frame.cols * frame.rows * frame.channels()];
        memcpy(pInput->data, frame.data, frame.cols * frame.rows * frame.channels());
        pInput->height = frame.rows;
        pInput->width = frame.cols;

        call_back(pInput, NULL, 0, (void *)arg);

        frame.release();
#endif
    }
}

//推送视频流
void CameraServer::post_thread(void *)
{
    sem_post_start.wait();
#ifdef _RTP
    H264Writer mwrite(camera_width, camera_height, "rtp_play.sdp", "rtp://192.168.1.200:8990");
#else
    H264Writer mwrite(camera_width, camera_height);
#endif
    for (;;) {
        if (is_stop) break;
        sem_post.wait();
        cv::Mat mat;
        {
            std::unique_lock<std::mutex> locker(mutex_show);
            mat = que_show_mat.front();
            que_show_mat.pop();
        }
        AVPacket pkt;
        int size = mwrite.addFrame(mat.data, pkt);
        
        if (size) {
#ifndef _RTP
            DataSend::getInstance()->send_stream(pkt.data, size);
#endif
            {
                mutex_pkt.lock();
                
                que_pkt.push(pkt);
                mutex_pkt.unlock();
                // sem_write.post();
            }
            // av_packet_unref(&pkt);
        }

        static uint64_t fps_num = 0;
        static auto t1 = get_current_mtime();
        fps_num++;
        if (fps_num % 30 == 0 && fps_num != 30) {
            auto t2 = get_current_mtime();
            auto fps = 30.f / (t2 - t1) * 1000;
            std::cout << "--------------send fps:" << 30.f / (t2 - t1) * 1000 << std::endl;
            t1 = t2;
            fflush(stdout);
        }
        
    }
}

class videowriter{
public:
    videowriter(){}
    bool open(const unsigned int width_, const unsigned int height_, const char *filename_)
    {

       int ret = avformat_alloc_output_context2(&fc_out, NULL, NULL, filename_);
       assert(ret >= 0);

       AVCodec *codec = avcodec_find_encoder_by_name("libx264");
       assert(codec);

       stream_out = avformat_new_stream(fc_out, codec);
       assert(stream_out);

        c = stream_out->codec;
        c->width = width_;
        c->height = height_;
        c->pix_fmt = AV_PIX_FMT_YUV420P;
        c->time_base = (AVRational){1, 25};
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        avcodec_parameters_from_context(stream_out->codecpar, c); 
        stream_out->time_base = (AVRational){1, 25};
        av_dump_format(fc_out, 0, filename_, 1);
        ret = avio_open(&fc_out->pb, filename_, AVIO_FLAG_READ_WRITE);
        if(ret<0){
            printf("output file open failed %s\n",filename_);
            return false;
        }    
        // //写头
        avformat_write_header(fc_out, NULL);
        return true;
    }
    int write(AVPacket &pkt)
    {        
        av_packet_rescale_ts(&pkt, (AVRational){1, 25}, stream_out->time_base);
        int ret = av_interleaved_write_frame(fc_out, &pkt);
        if(ret<0){
            printf("write packet failed!\n"); 
        }
        av_packet_unref(&pkt);
        return ret;
    }
    bool is_open()
    {
        if(fc_out){
            return true;
        }
        return false;
    }
    bool release(){
        av_write_trailer(fc_out);
        avio_closep(&fc_out->pb); 
        avcodec_close(stream_out->codec);
        avformat_free_context(fc_out);
        return true;
    }
private:
    AVFormatContext *fc_out;
    AVOutputFormat *fmt_out;
    AVStream *stream_out;
    AVCodecContext *c;
};

static std::string get_current_date()
{
	struct   tm     *ptm;
	long       ts;
	int         y,m,d,h,n,s;
	ts   =   time(NULL);
	ptm   =   localtime(&ts);
	y   =   ptm-> tm_year+1900;     //年
	m   =   ptm-> tm_mon+1;             //月
	d   =   ptm-> tm_mday;               //日
	h   =   ptm-> tm_hour;               //时
	n   =   ptm-> tm_min;                 //分
	s   =   ptm-> tm_sec;                 //秒
	return std::move(std::to_string(y)+"-"+std::to_string(m)+"-"+std::to_string(d)+"-"+std::to_string(h)
			+"-"+std::to_string(n)+"-"+std::to_string(s));
}

void CameraServer::save_thread(void *arg)
{
    sem_write_start.wait();
    std::string file_Str= get_current_date()+".mp4";
    videowriter writer;
    writer.open(camera_width, camera_height,file_Str.c_str());
    for (;;) {
        if (is_stop) break;
        sem_write.wait();
        AVPacket pkt;
        {
            std::unique_lock<std::mutex> locker(mutex_pkt);
            pkt = que_pkt.front();
            que_pkt.pop();
        }
        int ret = writer.write(pkt);       
        static int frame_id=0;
        frame_id++;
        if((frame_id%(25*60*60)==0)){//1分钟保存一个视频
            writer.release();
            printf("video write to %s successfully!\n", file_Str.c_str());
            file_Str = get_current_date()+".mp4";
            writer.open(camera_width, camera_height, file_Str.c_str());
        }
    }
    if(writer.is_open()){
        writer.release();
        printf("video write to %s successfully!\n", file_Str.c_str());
    }
}
