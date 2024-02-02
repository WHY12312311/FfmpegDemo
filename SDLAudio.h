// 使用SDL进行音频播放的函数，注意SDL会自动创建线程来控制音频的播放

#ifndef DEMOPLAYER_SDLAUDIO_H
#define DEMOPLAYER_SDLAUDIO_H

#include "player.h"

// 用于控制播放缓冲区的指针
static Uint8* audio_chuck;
static Uint32 audio_len;
static Uint8* audio_pos;

// 解析一部份音频文件，并将其放入缓冲区中，以供取用
void audio_decode(PlayerState* is) {
    AVFrame* frame = av_frame_alloc();
    // 指向缓冲区当前的尾部
    Uint8* audio_tail = audio_chuck;
    audio_pos = audio_chuck;

    AVPacket* pkt;
    is->auqueue->get_packet(pkt, true);

    if (avcodec_send_packet(is->pAuCodecCtx, pkt) < 0) {
        cout << "wrong sending packet to audio decoder" << endl;
        return;
    }
    while (avcodec_receive_frame(is->pAuCodecCtx, frame) == 0) {
        int len = swr_convert(is->swrCtx, &is->audio_buffer, is->audio_buffer_size,
                              (const uint8_t **)frame->data, frame->nb_samples);

        // int length = len * is->pAuCodecCtx->ch_layout.nb_channels;
        int length = is->audio_buffer_size;
        // memcpy(audio_tail, is->audio_buffer, length);
        audio_tail += length;
        audio_len += length;

        // 更新音频时钟
        int data_size = is->wanted_spec.channels * len * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        int n = 2 * is->pAuCodecCtx->ch_layout.nb_channels;
        is->audio_clock += (double)data_size /
                           (double)(n * is->pAuCodecCtx->sample_rate);
    }
    av_packet_unref(pkt);

    // 留出一部份空间来，防止剩下的空间不够用而队列那边又出队了
//    while (is->audio_buffer_size <= is->pcm_buffer_size - audio_len) {
//        AVPacket* pkt;
//        is->auqueue->get_packet(pkt, true);
//
//        if (avcodec_send_packet(is->pAuCodecCtx, pkt) < 0) {
//            cout << "wrong sending packet to audio decoder" << endl;
//            return;
//        }
//        while (avcodec_receive_frame(is->pAuCodecCtx, frame) == 0) {
//            int len = swr_convert(is->swrCtx, &is->audio_buffer, is->audio_buffer_size,
//                        (const uint8_t **)frame->data, frame->nb_samples);
//
//            // int length = len * is->pAuCodecCtx->ch_layout.nb_channels;
//            int length = is->audio_buffer_size;
//            memcpy(audio_tail, is->audio_buffer, length);
//            audio_tail += length;
//            audio_len += length;
//
//            // 更新音频时钟
//            int data_size = is->wanted_spec.channels * len * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
//            int n = 2 * is->pAuCodecCtx->ch_layout.nb_channels;
//            is->audio_clock += (double)data_size /
//                                    (double)(n * is->pAuCodecCtx->sample_rate);
//        }
//        av_packet_unref(pkt);
//    }
}

void fillAudio(void *userdata, Uint8* stream, int len) {
    auto is = (PlayerState*)userdata;
    // 如果缓冲区没有数据了，就去解码一部份来
    if (audio_len <= 0) {
        audio_decode(is);
        is->audio_index = 0;
    }

    SDL_memset(stream, 0, len);
    if (audio_len == 0)
        return;
    len = len > audio_len ? audio_len : len;    // 选输入和输出缓冲区大小较小的
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);

    audio_pos += len;
    audio_len -= len;
    is->audio_index += len;
}

void audio_init(PlayerState* is) {
    auto audio_para = is->pctx->streams[is->audioid]->codecpar;
    is->wanted_spec.freq = audio_para->sample_rate;
    is->wanted_spec.format = AUDIO_S16SYS;                                  // SYS代表会自动选取大端和小端值
    is->wanted_spec.channels = is->pAuCodecCtx->ch_layout.nb_channels;
    is->wanted_spec.silence = 0;                                            // 设置静音的值
    is->wanted_spec.samples = is->nb_sample;                                // 样本数，可以理解为每一帧数据中的值
    is->wanted_spec.callback = fillAudio;                                   // 注册回调函数
    is->wanted_spec.userdata = is;                                          // 回调函数调用时自动传递的参数

    SDL_OpenAudio(&is->wanted_spec, NULL);

    // 指针指向初始化好的缓冲区，这时候应该是空的
    audio_chuck = (Uint8*)is->audio_buffer;
    audio_len = 0;
    audio_pos = audio_chuck;

    // SDL_PauseAudio(0);
}

#endif //DEMOPLAYER_SDLAUDIO_H
