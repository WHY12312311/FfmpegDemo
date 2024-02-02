// 定义线程安全的队列，主要用于保存解复用出来的音频和视频包
#ifndef DEMOPLAYER_DATAQUEUE_H
#define DEMOPLAYER_DATAQUEUE_H

#include <queue>
#include "player.h"

using namespace std;

class PacketQueue {
public:
    PacketQueue() {
        mutex = SDL_CreateMutex();
        cond = SDL_CreateCond();
    }
    ~PacketQueue() {
        SDL_DestroyMutex(mutex);
        SDL_DestroyCond(cond);
    }

    int push_packet(AVPacket*);
    int get_packet(AVPacket*&, bool);
    auto size() {return q.size();}

private:
    queue<AVPacket*> q; // 尝试直接使用queue来维护队列
    SDL_mutex* mutex;
    SDL_cond* cond;
};

int PacketQueue::push_packet(AVPacket* pkt) {
    SDL_LockMutex(mutex);
    while (q.size() > MAX_QUEUE_SIZE) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    AVPacket* newPkt = av_packet_alloc();
    int ret = av_packet_ref(newPkt, pkt);
    if (ret < 0)
        return -1;

    // 向队列中插入数据
    SDL_LockMutex(mutex);
    q.push(newPkt);
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);

    return 0;
}

int PacketQueue::get_packet(AVPacket*& pkt, bool block) {
    int ret = 0;
    SDL_LockMutex(mutex);
    while (1) {
        if (q.size()) {
            pkt = q.front();
            q.pop();
            ret = 0;
            break;
        } else if (!block) {
            ret = -1;
            break;
        } else {
            SDL_CondWait(cond, mutex);
        }
    }
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);

    return ret;
}

// 解码后的视频帧传入队列，这是解码线程做的
void pictq_push(PlayerState* is, AVFrame* pFrame) {
    // 初始化转换器输出缓冲区
    auto curr_frame = av_frame_alloc();
    auto out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, is->pViCodecCtx->width, is->pViCodecCtx->height, 1));
    av_image_fill_arrays(curr_frame->data, curr_frame->linesize, out_buffer,
                         AV_PIX_FMT_YUV420P, is->pViCodecCtx->width, is->pViCodecCtx->height, 1);

    // 转换为SDL可以识别的帧格式
    sws_scale(is->ImgCvtCtx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pFrame->height,
              curr_frame->data, curr_frame->linesize);
    curr_frame->pts = pFrame->pts;

    SDL_LockMutex(is->pict_mutex);
    is->pictq->push(curr_frame);
    SDL_CondSignal(is->pict_cond);
    SDL_UnlockMutex(is->pict_mutex);
}

#endif //DEMOPLAYER_DATAQUEUE_H
