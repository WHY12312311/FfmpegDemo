// 定义全局数据和一些include

#ifndef DEMOPLAYER_DEFINE_H
#define DEMOPLAYER_DEFINE_H

#include <iostream>
#include <queue>

extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <libavcodec//bsf.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>
}

using namespace std;

#define MAX_QUEUE_SIZE 4096

class PacketQueue; // 前向声明

struct VideoFrame {
    AVFrame* vp;
    int64_t pts;
};

struct PlayerState {
    // 文件基本信息与指针
    char* filename;
    AVFormatContext* pctx;

    // 流信息
    AVStream* vStream;
    AVStream* aStream;
    int videoid;
    int audioid;

    // 视频解码相关
    const AVCodec* video_codec;
    AVCodecContext* pViCodecCtx;
    SwsContext* ImgCvtCtx;          // 视频格式转换器

    // 音频解码相关
    const AVCodec* audio_codec ;
    AVCodecContext* pAuCodecCtx;
    SwrContext *swrCtx;             // 音频重采样器
    int audio_buffer_size;
    uint8_t* audio_buffer;

    // 音频基本信息


    // 线程相关
    SDL_Thread* demux_tid;
    SDL_Thread* video_tid;

    // packet队列和帧队列
    PacketQueue* viqueue;
    PacketQueue* auqueue;
    queue<AVFrame*>* pictq;

    // 视频帧队列不封装了，这是锁和条件变量
    SDL_mutex* pict_mutex;
    SDL_cond* pict_cond;

    // 音频播放器与播放缓冲区
    SDL_AudioSpec wanted_spec;
    int pcm_buffer_size;
    char* pcm_buffer;
    int nb_sample;

    // 各种时间戳
    int64_t vpts_prev;
    int64_t vpts_curr;
    double audio_clock;
    unsigned int audio_index;

    // 标识符
    bool isAudioOn;
    bool isShut;
} ;

// 初始化播放器，即填充PlayerState对象
static PlayerState* player_init(const char*);
void player_shut_down(PlayerState*);

void player_running(const char*);

#endif //DEMOPLAYER_DEFINE_H
