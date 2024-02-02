// 定义SDL播放器的操作，包括初始化、视频播放、音频播放等

#ifndef DEMOPLAYER_SDLVIDEO_H
#define DEMOPLAYER_SDLVIDEO_H

#include "player.h"

double sync_th = 10;

void init_SDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
        printf("There is something wrong with your SDL Libs. Couldn't run");
}

double get_audio_clock(PlayerState* is) {
    double pts=is->audio_clock;//Maintained in the audio thread，取得解码操作完成时的当前播放时间戳
    //还未(送入声卡)播放的剩余原始音频数据长度，等于解码后的多帧原始音频数据长度-累计送入声卡的长度
    int hw_buf_size=is->audio_buffer_size-is->audio_index;//计算当前音频解码数据缓存索引位置
    int bytes_per_sec=0;//每秒的原始音频字节数
    int pcm_bytes=is->pAuCodecCtx->ch_layout.nb_channels * 2;//每组原始音频数据字节数=声道数*每声道数据字节数
    bytes_per_sec=is->pAuCodecCtx->sample_rate * pcm_bytes;//计算每秒的原始音频字节数
    if (bytes_per_sec) {//检查每秒的原始音频字节数是否有效
        pts-=(double)hw_buf_size/bytes_per_sec;//根据送入声卡缓存的索引位置，往前倒推计算当前时刻的音频播放时间戳pts
    }
    return pts;//返回当前正在播放的音频时间戳
}


// 计算出延迟的时间
double get_delay(PlayerState* is) {
    double vcurr = is->vpts_curr * av_q2d(is->vStream->time_base);
    double vprev = is->vpts_prev * av_q2d(is->vStream->time_base);

    // double acurr = is->apts_curr * av_q2d(is->vStream->time_base);
    double acurr = get_audio_clock(is);

    double delay = (vcurr - vprev) * 1000;
    double diff = (vcurr - acurr) * 1000;

    if (fabs(diff) < sync_th) {
        return 0.0;
    } else if (diff < -sync_th) {
        delay += diff;
    } else {
        delay *= 2;
    }


    return delay;
}

// 当视频播放过慢，就丢掉一系列帧
void video_drop_frame(PlayerState* is) {
    int num = 30;
    SDL_LockMutex(is->pict_mutex);
    while (is->pictq->size() && num--) {
        is->vpts_curr = is->pictq->front()->pts;
        is->pictq->pop();
    }
    is->vpts_prev = is->vpts_curr;
    SDL_UnlockMutex(is->pict_mutex);
}

void video_display(PlayerState* is) {
    SDL_Window* pSDLWindow = NULL;
    SDL_Renderer* pSDLRender = NULL;
    SDL_Texture* pSDLTexture = NULL;
    SDL_Rect SDLRect;
    SDL_Event SDLEvent;
    int frameLen;
    int videoWidth = is->pViCodecCtx->width;
    int videoHeight = is->pViCodecCtx->height;
    unsigned char* pBuf = NULL;
    Uint32 cnt = 0;

    frameLen = videoWidth * videoHeight * 3 / 2;    // Y + 0.25U + 0.25V
    pBuf = (unsigned char *)malloc(frameLen);   // 缓冲区
    if (!pBuf) {
        cout << "wrong calling malloc" << endl;
        return;
    }

    // 初始化窗口渲染器等
    SDL_Init(SDL_INIT_VIDEO);
    pSDLWindow = SDL_CreateWindow("WHY player", 0, 0, videoWidth, videoHeight,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!pSDLWindow) {
        cout << "wrong creating window" << endl;
        return;
    }
    pSDLRender = SDL_CreateRenderer(pSDLWindow, -1, 0);
    if (!pSDLRender) {
        cout << "wrong creating render" << endl;
        return;
    }
    pSDLTexture = SDL_CreateTexture(pSDLRender, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                    videoWidth, videoHeight);
    SDLRect = {0, 0, videoWidth, videoHeight};

    // 循环中如果没有事件，可能会被os认为是卡死的程序，不予显示窗口
    while (!is->isShut) {
        while (SDL_PollEvent(&SDLEvent)) {
            if (SDLEvent.type == SDL_QUIT)
                is->isShut = true;
        }

        SDL_LockMutex(is->pict_mutex);
//        while (is->pictq->empty())
//        SDL_CondWait(is->pict_cond, is->pict_mutex);
        if (is->pictq->empty()) // wait的话，播放完了就一直wait了，关不上
            continue;
        auto vp = is->pictq->front();
        is->pictq->pop();
        SDL_UnlockMutex(is->pict_mutex);

        // 尽量晚地打开声音，减少一开始突然加速的情况
        if (!is->isAudioOn) {
            SDL_PauseAudio(0);
            is->isAudioOn = true;
        }

        // 设置延迟时间，以便音视频同步
        is->vpts_prev = is->vpts_curr;
        is->vpts_curr = vp->pts;

        // 渲染时间大概是2-5ms
        double delay_time = get_delay(is);
        int delay = (int)delay_time - 4;
        if (delay < -1000 && cnt >= 10) {
            video_drop_frame(is);
            cnt = 0;
        } else if (delay > 0) {
            SDL_Delay(delay);
        } else {
            cnt++;
        }

        SDL_UpdateYUVTexture(pSDLTexture, NULL,
                             vp->data[0], vp->linesize[0],
                             vp->data[1], vp->linesize[1],
                             vp->data[2], vp->linesize[2]);
        SDL_RenderClear(pSDLRender);
        SDL_RenderCopy(pSDLRender, pSDLTexture, NULL, NULL);
        SDL_RenderPresent(pSDLRender);
    }
}

#endif //DEMOPLAYER_SDLVIDEO_H
