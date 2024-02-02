// 定义解引用线程

#ifndef DEMOPLAYER_THREAD_H
#define DEMOPLAYER_THREAD_H

#include "player.h"
#include "dataqueue.h"

// 解引用线程，不停地将音视频解复用出数据
// 并将解码后的数据放入相应的队列中
// 按理说初始化都是player里面完成了的，所以这里直接解复用
int demux_thread(void* arg) {
    auto is = (PlayerState*)arg;
    AVPacket* pkt = av_packet_alloc();
    int ret = 0;
    while (av_read_frame(is->pctx, pkt) == 0 && !is->isShut) {
        if (pkt->stream_index == is->videoid) { // 处理视频格式
            ret = is->viqueue->push_packet(pkt);
        } else if (pkt->stream_index == is->audioid) {  // 处理音频格式
            ret = is->auqueue->push_packet(pkt);
        }
        av_packet_unref(pkt);
        if (ret)    // 出错
            return ret;
    }
    return ret;
}

// 视频解码线程，从解复用线程对应的队列中取出pkt并播放
int video_decode_thread(void* arg) {
    auto is = (PlayerState*)arg;
    while (is && !is->isShut) {
        // 阻塞地从队列中取出packet
        AVPacket* pkt;
        AVFrame* frame = av_frame_alloc();
        is->viqueue->get_packet(pkt, true);

        int ret = 0;
        if ((ret = avcodec_send_packet(is->pViCodecCtx, pkt)) < 0) {
            char error_buf[1024]; // 错误信息缓冲
            av_strerror(ret, error_buf, sizeof(error_buf));
            cout << error_buf << endl;
            return ret;
        }
        while (avcodec_receive_frame(is->pViCodecCtx, frame) == 0) {
            pictq_push(is, frame);
        }
        av_packet_unref(pkt);
    }

    return 0;
}

#endif //DEMOPLAYER_THREAD_H
