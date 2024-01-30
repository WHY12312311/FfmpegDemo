// 使用SDL2库播放解码后的音视频

#ifndef DEMOPLAYER_SDLPLAYER_H
#define DEMOPLAYER_SDLPLAYER_H
#include <iostream>
#include <string>

extern "C" {
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <SDL2/SDL.h>
}

using namespace std;

// 用于控制播放缓冲区的指针
static Uint8* audio_chuck;
static Uint32 audio_len;
static Uint8* audio_pos;

// 回调函数，当音频缓冲区空的时候会自动调用
// 注意，函数中的参数是不需要我们手动传入的，即便是stream这个指向播放音频缓冲区的指针
// 这个地方的缓冲区也是不需要我们申请的，sdl库自动申请管理释放这个缓冲区
// 在这个缓冲区为空的时候，会自动地调用这个回调函数，通过SDL_MixAudio将数据输入缓冲区并播放
void fillAudio(void *udata, Uint8* stream, int len) {
    SDL_memset(stream, 0, len);
    if (audio_len == 0)
        return;
    len = len > audio_len ? audio_len : len;    // 选输入和输出缓冲区大小较小的
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

// pcm播放函数
void pcm_player(char* filename) {
    auto dirtmp = dirname(filename);
    string dirname(dirtmp);
    FILE* audiofp = fopen((dirname+"/audio.pcm").c_str(), "r");

    SDL_Init(SDL_INIT_AUDIO);

    // 设置播放的相关参数
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 44100;
    wanted_spec.format = AUDIO_S16SYS;  // SYS代表会自动选取大端和小端值
    wanted_spec.channels = 2;
    wanted_spec.silence = 0;            // 设置静音的值
    wanted_spec.samples = 1024;         // 样本数，可以理解为每一帧数据中的值
    wanted_spec.callback = fillAudio;   // 注册回调函数

    SDL_OpenAudio(&wanted_spec, NULL);

    int pcm_buffer_size = 4096;
    char* pcm_buffer = (char*)malloc(pcm_buffer_size);

    // 该函数参数为正书的时候，表示暂停一定时间为0则表示开始播放
    // 另外播放是sdl开了另一个线程来做的，并不是当前这个线程做的
    SDL_PauseAudio(0);

    int cnt = 0;
    while (cnt < 3) {
        // 当执行这段代码的时候，就代表pcm_buffer空了，这时候从文件中读取数据放进去
        int buffer_size = 0;
        if ((buffer_size = fread(pcm_buffer, 1, pcm_buffer_size, audiofp)) != pcm_buffer_size) {
            // 这个时候是读到头了，让它从头开始
            fseek(audiofp, 0, SEEK_SET);
            // fread(pcm_buffer, 1, pcm_buffer_size, audiofp);
            cnt++;
        }

        audio_chuck = (Uint8*)pcm_buffer;
        audio_len = buffer_size;
        audio_pos = audio_chuck;

        // 这段代码保证一点：只有pcm_buffer空的时候才从文件中读取数据并
        // 将其放入pcm_buffer中
        // 如果没有这个循环一直检查，就会在pcm_buffer有数据的时候进行上面
        // 的数据读取，会覆盖pcm_buffer，所以实际上是线程同步的过程
        while (audio_len > 0)
            SDL_Delay(1);
    }

    // 收尾工作
    if (audiofp)
        fclose(audiofp);
    if (pcm_buffer)
        free(pcm_buffer);
}


// yuv播放函数
void yuv_player(char* filename) {
    auto dirtmp = dirname(filename);
    string dirname(dirtmp);
    FILE* videofp = fopen((dirname+"/video.yuv").c_str(), "r");

    SDL_Window* pSDLWindow = NULL;
    SDL_Renderer* pSDLRender = NULL;
    SDL_Texture* pSDLTexture = NULL;
    SDL_Rect SDLRect;
    SDL_Event SDLEvent;
    int frameLen;
    int videoWidth = 1080;
    int videoHeight = 1920;
    unsigned char* pBuf = NULL;
    frameLen = videoWidth * videoHeight * 3 / 2;    // Y + 0.25U + 0.25V
    pBuf = (unsigned char *)malloc(frameLen);   // 缓冲区
    if (!pBuf) {
        cout << "wrong calling malloc" << endl;
        return;
    }

    // 初始化窗口渲染器等
    SDL_Init(SDL_INIT_VIDEO);
    pSDLWindow = SDL_CreateWindow("YUV player", 0, 0, videoWidth, videoHeight,
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
    // 设置背景色
    SDL_SetRenderDrawColor(pSDLRender, 0, 255, 0, 255);
    SDL_RenderClear(pSDLRender);
    SDL_RenderPresent(pSDLRender);

    // 循环中如果没有事件，可能会被os认为是卡死的程序，不予显示窗口
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&SDLEvent)) {
            if (SDLEvent.type == SDL_QUIT)
                quit = true;
        }
        if (fread(pBuf, 1, frameLen, videofp) < 0)
            quit = true;

        SDL_UpdateTexture(pSDLTexture, NULL, pBuf, videoWidth);
        SDL_RenderClear(pSDLRender);
        SDL_RenderCopy(pSDLRender, pSDLTexture, NULL, NULL);
        SDL_RenderPresent(pSDLRender);
        SDL_Delay(1000/30); // 30帧，但可能要留出处理时间来
    }

    // 收尾工作
    if (videofp)
        fclose(videofp);
    if (pBuf)
        free(pBuf);
    if (pSDLTexture)
        SDL_DestroyTexture(pSDLTexture);
    if (pSDLRender)
        SDL_DestroyRenderer(pSDLRender);
    if (pSDLWindow)
        SDL_DestroyWindow(pSDLWindow);
    SDL_Quit();
}


#endif //DEMOPLAYER_SDLPLAYER_H
