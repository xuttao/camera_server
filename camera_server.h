#pragma once
#define _DOUBLE_V7

#include "dataModel.h"

class CameraServer
{

public:
    CameraServer();
    ~CameraServer();
	void run();
private:
    void slot_btn();
    void slot_timer();
    void slot_con();

    static void receive_thread(void *);
    static void input_thread(void *);
    static void post_thread(void *);
    static void save_thread(void *arg);
    static void request_thread(void *);

private:
    // QTimer timer;
    bool is_play = false;
};
