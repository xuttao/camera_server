#include "h264writer.h"
#include <assert.h>

const char *out_file = "a.mp4";
H264Writer::H264Writer(const unsigned int width_, const unsigned int height_, const char *filename_, const char *url) : width(width_), height(height_), iframe(0)

{
    swsCtx = sws_getContext(width, height,
                            AV_PIX_FMT_RGB24, width, height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    assert(swsCtx);
#ifndef _RTP
    const char *fmtext = "play.264";
    fmt = av_guess_format(fmtext, NULL, NULL);
    int ret = avformat_alloc_output_context2(&fc, NULL, NULL, fmtext);
    assert(ret >= 0);
#else
    fmt = av_guess_format("rtp", NULL, NULL);
    int ret = avformat_alloc_output_context2(&fc, fmt, fmt->name, url);
    assert(ret >= 0);
#endif

     // Setting up the codec.
    AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    assert(codec);


    AVDictionary *opt = NULL;
    av_dict_set(&opt, "preset", "ultrafast", 0);
    av_dict_set(&opt, "crf", "25", 0);
    stream = avformat_new_stream(fc, codec);
    assert(stream);


    c = stream->codec;
    c->width = width;
    c->height = height;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->time_base = (AVRational){1, FRAME};
    // c->bit_rate=40000;

    if (fc->oformat->flags & AVFMT_GLOBALHEADER) {
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    ret = avcodec_open2(c, codec, &opt);
    assert(ret == 0);
    av_dict_free(&opt);
    stream->time_base = (AVRational){1, FRAME};

#ifdef _RTP
    av_dump_format(fc, 0, fc->filename, 1);
    ret = avio_open(&fc->pb, fc->filename, AVIO_FLAG_WRITE);
    assert(ret >= 0);

    ret = avformat_write_header(fc, &opt);
    assert(ret >= 0);
    {
        char buf[1024 * 10];
        AVFormatContext *ac[] = {fc};
        int ret = av_sdp_create(ac, 1, buf, 1024 * 10);
        // assert(ret == 0);

        printf("sdp:%s", buf);
        FILE *fsdp = fopen(filename_, "w");
        fprintf(fsdp, "%s", buf);
        fclose(fsdp);
    }
#else
    // av_dump_format(fc, 0, fmtext, 1);
    // avio_open(&fc->pb, fmtext, AVIO_FLAG_WRITE);
#endif
    

    av_dict_free(&opt);
    rgbpic = av_frame_alloc();
    rgbpic->format = AV_PIX_FMT_RGB24;
    rgbpic->width = width;
    rgbpic->height = height;
    ret = av_frame_get_buffer(rgbpic, 1);
    yuvpic = av_frame_alloc();
    yuvpic->format = AV_PIX_FMT_YUV420P;
    yuvpic->width = width;
    yuvpic->height = height;
    ret = av_frame_get_buffer(yuvpic, 1);
    assert(ret == 0);
}

int H264Writer::addFrame(const uint8_t *pixels, AVPacket &pkt)
{
    rgbpic->data[0] = const_cast<uint8_t *>(pixels);
    sws_scale(swsCtx, rgbpic->data, rgbpic->linesize, 0,
              height, yuvpic->data, yuvpic->linesize);

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    int res = 0;
    yuvpic->pts = iframe;

    int got_output;
    int ret = avcodec_encode_video2(c, &pkt, yuvpic, &got_output);
    if (got_output) {
        av_packet_rescale_ts(&pkt, (AVRational){1, FRAME}, stream->time_base);

        pkt.stream_index = stream->index;
        iframe++;
        res = pkt.size;
        
#ifdef _RTP
        av_interleaved_write_frame(fc, &pkt);
#endif
        
        // av_packet_unref(&pkt);
       
    }
    return res;
}

H264Writer::~H264Writer()
{
    for (int got_output = 1; got_output;) {
        int ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
        if (got_output) {
            fflush(stdout);
            av_packet_rescale_ts(&pkt, (AVRational){1, FRAME}, stream->time_base);
            pkt.stream_index = stream->index;
            printf("Writing frame %d (size = %d)\n", iframe++, pkt.size);
#ifdef _RTP
            av_interleaved_write_frame(fc, &pkt);
#endif
            av_packet_unref(&pkt);
        }
    }
    av_write_trailer(fc);
    if (!(fmt->flags & AVFMT_NOFILE)){
        avio_closep(&fc->pb);
        
    }
    avcodec_close(stream->codec);
    sws_freeContext(swsCtx);
    av_frame_free(&rgbpic);
    av_frame_free(&yuvpic);
    avformat_free_context(fc);
}
