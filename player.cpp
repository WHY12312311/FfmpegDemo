#include "player.h"
#include "dataqueue.h"
#include "thread.h"
#include "SDLVideo.h"
#include "SDLAudio.h"

using namespace std;

static PlayerState* player_init(const char* input_file) {
    auto is = new PlayerState;
    is->isShut = false;
    is->filename = av_strdup(input_file);
    is->pctx = NULL;

    // 读取文件基本信息
    if (avformat_open_input(&is->pctx, is->filename, NULL, NULL) != 0) {
        cout << "wrong opening input file" << endl;
        return NULL;
    }
    if (avformat_find_stream_info(is->pctx, NULL) < 0) {
        cout << "wrong getting video info" << endl;
        return NULL;
    }

    // 获取视频流和音频流编号
    is->videoid = av_find_best_stream(is->pctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (is->videoid < 0) {
        cout << "no video stream found" << endl;
        return NULL;
    }
    is->audioid = av_find_best_stream(is->pctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (is->audioid < 0) {
        cout << "no audio stream found" << endl;
        return NULL;
    }
    is->vStream = is->pctx->streams[is->videoid];
    is->aStream = is->pctx->streams[is->audioid];

    // 初始化视频解码相关
    is->video_codec = avcodec_find_decoder(is->pctx->streams[is->videoid]->codecpar->codec_id);
    is->pViCodecCtx = avcodec_alloc_context3(is->video_codec);
    avcodec_parameters_to_context(is->pViCodecCtx, is->pctx->streams[is->videoid]->codecpar);
    if (avcodec_open2(is->pViCodecCtx, is->video_codec, NULL) < 0) {
        cout << "wrong opening video decoder" << endl;
        return NULL;
    }
    is->ImgCvtCtx = sws_getContext(is->pViCodecCtx->width, is->pViCodecCtx->height, is->pViCodecCtx->pix_fmt,
                                   is->pViCodecCtx->width, is->pViCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC,
                               NULL, NULL, NULL);

    // 初始化音频解码相关
    is->audio_codec = avcodec_find_decoder(is->pctx->streams[is->audioid]->codecpar->codec_id);
    is->pAuCodecCtx = avcodec_alloc_context3(is->audio_codec);
    avcodec_parameters_to_context(is->pAuCodecCtx, is->pctx->streams[is->audioid]->codecpar);
    if (avcodec_open2(is->pAuCodecCtx, is->audio_codec, NULL) < 0) {
        cout << "wrong opening audio decoder" << endl;
        return NULL;
    }

    // 初始化重采样器
    auto audio_para = is->pctx->streams[is->audioid]->codecpar;
    AVChannelLayout out_channel_layout = audio_para->ch_layout;
//    enum AVSampleFormat out_sampleFormat = is->pAuCodecCtx->sample_fmt;
    enum AVSampleFormat out_sampleFormat = AV_SAMPLE_FMT_S16;
    // int out_sample_rate = audio_para->sample_rate;
    int out_sample_rate = audio_para->sample_rate;
    int out_nb_channels = out_channel_layout.nb_channels;
    AVChannelLayout in_channel_layout = audio_para->ch_layout;

    is->swrCtx = swr_alloc();
    swr_alloc_set_opts2(&is->swrCtx, &out_channel_layout, out_sampleFormat, out_sample_rate, &in_channel_layout,
                        is->pAuCodecCtx->sample_fmt, is->pAuCodecCtx->sample_rate, 0, NULL);
    swr_init(is->swrCtx);

    is->nb_sample = 1024;   // 采样个数
    if (is->pAuCodecCtx->sample_fmt == AV_SAMPLE_FMT_FLTP ||
            is->pAuCodecCtx->sample_fmt == AV_SAMPLE_FMT_FLT)
        is->nb_sample = 1152;
    is->audio_buffer_size = av_samples_get_buffer_size(NULL, out_nb_channels, is->nb_sample, out_sampleFormat, 1);
    is->audio_buffer = (uint8_t*)av_malloc(is->audio_buffer_size * out_nb_channels);

    // 初始化队列
    is->viqueue = new PacketQueue;
    is->auqueue = new PacketQueue;
    is->pictq = new queue<AVFrame*>;

    // 初始化视频帧队列的锁和条件变量
    is->pict_mutex = SDL_CreateMutex();
    is->pict_cond = SDL_CreateCond();

    // 初始化音频播放缓冲区
    is->pcm_buffer_size = is->audio_buffer_size * out_nb_channels;
    is->pcm_buffer = (char*)av_malloc(is->pcm_buffer_size * 2);

    // 同步相关
    is->audio_clock = 0.0;

    is->isAudioOn = false;

    return is;
}

void player_shut_down(PlayerState* is) {
    if (is) {
        is->isShut = true;
        SDL_Delay(10);

        delete is->viqueue;
        delete is->auqueue;
        delete is->pictq;
        delete is->pcm_buffer;

        SDL_DetachThread(is->demux_tid);
        SDL_DetachThread(is->video_tid);

        SDL_DestroyMutex(is->pict_mutex);
        SDL_DestroyCond(is->pict_cond);

        delete is;
        is = nullptr;
    }
}

void player_running(const char* input_file) {
    //  初始化
    auto is = player_init(input_file);

    // 开启解引用线程，填充两个队列
    is->demux_tid = SDL_CreateThread(demux_thread, "demux thread", is);
    SDL_Delay(10);  // 稍等一下解引用的生产者线程

    // 开启视频解码线程，解码后帧放入队列中
    is->video_tid = SDL_CreateThread(video_decode_thread, "video decode thread", is);

    init_SDL();

    // 开启音频播放，注意是SDL内部的线程管理的
    audio_init(is);

    video_display(is);
    // while (1) {}

    player_shut_down(is);
}