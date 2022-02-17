#ifndef _H264WRITER_H_
#define _H264WRITER_H_

#include <stdint.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <x264.h>
}

class H264Writer
{
    const unsigned int width, height;
    int64_t iframe;

    SwsContext *swsCtx;
    AVOutputFormat *fmt;
    AVStream *stream;
    AVFormatContext *fc;

    AVCodecContext *c;
    AVPacket pkt;
    AVFrame *rgbpic, *yuvpic;
    static const int FRAME = 25;

public:
    H264Writer(const unsigned int width, const unsigned int height, const char *filename = NULL, const char *url = NULL);

    int addFrame(const uint8_t *pixels, AVPacket &pkt);

    ~H264Writer();
};

#endif
