// 完成对音视频的解码和解复用

#ifndef DEMOPLAYER_DEMODECODE_H
#define DEMOPLAYER_DEMODECODE_H

#include <iostream>

extern "C" {
#include <stdio.h>
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
}

using namespace std;

void get_info(const char* filename) {
    AVFormatContext* pctx = NULL;
    if (avformat_open_input(&pctx, filename, NULL, NULL) != 0) {
        cout << "wrong opening input file" << endl;
        return;
    }
    if (avformat_find_stream_info(pctx, NULL) < 0) {
        cout << "wrong getting video info" << endl;
        return;
    }

    for (int i = 0; i < pctx->nb_streams; i++) {
        auto type = pctx->streams[i]->codecpar->codec_type;
        std::string stream_type;
        switch (type) {
            case AVMEDIA_TYPE_VIDEO:
                stream_type = "video";
                break;
            case AVMEDIA_TYPE_AUDIO:
                stream_type = "audio";
                break;
        }
        const char* codec_name = avcodec_get_name(pctx->streams[i]->codecpar->codec_id);
        std::cout << "流" << i << ":" << std::endl;
        std::cout << "类型为：" << stream_type << std::endl;
        std::cout << "编码格式为：" << codec_name << std::endl;
        std::cout << "视频长度：" << pctx->duration << "ms" << std::endl;

        if (type == AVMEDIA_TYPE_VIDEO) {
            auto frame_rate = pctx->streams[i]->avg_frame_rate;
            std::cout << "视频帧率为：" << frame_rate.num << "/" << frame_rate.den << std::endl;
            std::cout << "视频码率为：" << pctx->streams[i]->codecpar->bit_rate << std::endl;
        } else if (type == AVMEDIA_TYPE_AUDIO) {
            auto audioinfo = pctx->streams[i]->codecpar;
            std::cout << "音频声道数为：" << audioinfo->ch_layout.nb_channels << std::endl;
            std::cout << "音频采样率为：" << audioinfo->sample_rate << "Hz" << std::endl;
        }

        std::cout << "------------------------" << std::endl;
    }
}

void player_decode(char* filename) {
    // 创建输出文件
    auto dirtmp = dirname(filename);
    string dirname(dirtmp);
    FILE* yuvfp = fopen((dirname + "/video.yuv").c_str(), "wb");
    FILE* pcmfp = fopen((dirname + "/audio.pcm").c_str(), "wb");
    AVFormatContext* pctx;
    // packets & frames
    AVPacket* pkt;
    AVFrame* frame, * frame_yuv;
    // h264 filter
    const AVBitStreamFilter* vifilter;
    AVBSFContext * vi_ctx;  // context
    // acc decoder
    const AVCodec* audio_codec ;
    AVCodecContext* pAuCodecCtx;
    // h264 decoder
    const AVCodec* video_codec;
    AVCodecContext* pViCodecCtx;
    SwsContext* ImgCvtCtx;
    // audio converter
    SwrContext *swrCtx;


    // 读取视频文件基本信息
    pctx = NULL;
    if (avformat_open_input(&pctx, filename, NULL, NULL) != 0) {
        cout << "wrong opening input file" << endl;
        return;
    }
    if (avformat_find_stream_info(pctx, NULL) < 0) {
        cout << "wrong getting video info" << endl;
        return;
    }

    // 视频和音频解复用、过滤和解码
    int videoid = av_find_best_stream(pctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoid < 0) {
        cout << "no video stream found" << endl;
        return;
    }

    int audioid = av_find_best_stream(pctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audioid < 0) {
        cout << "no audio stream found" << endl;
        return;
    }

    //  初始化packet和frame
    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    frame_yuv = av_frame_alloc();

    // 初始化h264解码器
    video_codec = avcodec_find_decoder(pctx->streams[videoid]->codecpar->codec_id);
    pViCodecCtx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(pViCodecCtx, pctx->streams[videoid]->codecpar);
    if (avcodec_open2(pViCodecCtx, video_codec, NULL) < 0) {
        cout << "wrong opening video decoder" << endl;
        return;
    }

    // 初始化yuv格式转换器
    ImgCvtCtx = sws_getContext(pViCodecCtx->width, pViCodecCtx->height, pViCodecCtx->pix_fmt,
                               pViCodecCtx->width, pViCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC,
                               NULL, NULL, NULL);

    // 初始化aac解码器
    audio_codec = avcodec_find_decoder(pctx->streams[audioid]->codecpar->codec_id);
    pAuCodecCtx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(pAuCodecCtx, pctx->streams[audioid]->codecpar);
    if (avcodec_open2(pAuCodecCtx, audio_codec, NULL) < 0) {
        cout << "wrong opening audio decoder" << endl;
        return;
    }


    // 设置pcm转码前后格式与初始化重采样转换器
    auto audio_para = pctx->streams[audioid]->codecpar;
    AVChannelLayout out_channel_layout;
    av_channel_layout_from_mask(&out_channel_layout, AV_CH_LAYOUT_STEREO);
    int out_nb_sample = 1024;   // 采样个数
    enum AVSampleFormat out_sampleFormat = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int out_nb_channels = out_channel_layout.nb_channels;
    AVChannelLayout in_channel_layout = audio_para->ch_layout;

    swrCtx = swr_alloc();
    swr_alloc_set_opts2(&swrCtx, &out_channel_layout, out_sampleFormat, out_sample_rate, &in_channel_layout,
            pAuCodecCtx->sample_fmt, pAuCodecCtx->sample_rate, 0, NULL);

    swr_init(swrCtx);
    // buffer，用于转换输出的缓冲区
    int buffer_size = av_samples_get_buffer_size(NULL, out_nb_channels, out_nb_sample, out_sampleFormat, 1);
    auto buffer = (uint8_t*)av_malloc(buffer_size * 2);


    // 给frame分配一定空间，作为容器
    auto out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pViCodecCtx->width, pViCodecCtx->height, 1));
    av_image_fill_arrays(frame_yuv->data, frame_yuv->linesize, out_buffer,
    AV_PIX_FMT_YUV420P, pViCodecCtx->width, pViCodecCtx->height, 1);



    // 逐个读取帧，按照帧的类型来决定处理方法
    while (av_read_frame(pctx, pkt) >= 0) {
        if (pkt->stream_index == videoid) { // 处理视频格式
            // 将pkt解码，获取yuv数据，输出到yuv文件中
            if (avcodec_send_packet(pViCodecCtx, pkt) < 0) {
                cout << "wrong sending packet to video decoder" << endl;
                return;
            }
            while (avcodec_receive_frame(pViCodecCtx, frame) == 0) {
                sws_scale(ImgCvtCtx, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height,
                frame_yuv->data, frame_yuv->linesize);
                int y_size = pViCodecCtx->width * pViCodecCtx->height;
                fwrite(frame_yuv->data[0],1,y_size,yuvfp);    //Y
                fwrite(frame_yuv->data[1],1,y_size/4,yuvfp);  //U
                fwrite(frame_yuv->data[2],1,y_size/4,yuvfp);  //V
            }
            av_packet_unref(pkt);
        } else if (pkt->stream_index == audioid) {  // 处理音频格式
            // 将pkt解码，生成pcm数据保存
            if (avcodec_send_packet(pAuCodecCtx, pkt) < 0) {
                cout << "wrong sending packet to audio decoder" << endl;
                return;
            }
            while (avcodec_receive_frame(pAuCodecCtx, frame) == 0) {
                swr_convert(swrCtx, &buffer, buffer_size, (const uint8_t **)frame->data, frame->nb_samples);
                fwrite(buffer, 1, buffer_size, pcmfp);
            }
            av_packet_unref(pkt);
        }
    }
    // 收尾工作
    if (yuvfp)
    fclose(yuvfp);
    if (pkt)
    av_packet_free(&pkt);
    if (frame)
    av_frame_free(&frame);
    if (frame_yuv)
    av_frame_free(&frame_yuv);
}

#endif //DEMOPLAYER_DEMODECODE_H
