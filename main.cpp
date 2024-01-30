#include <iostream>
#include "DemoDecode.h"
#include "SDLPlayer.h"

int main(int argc, char** argv) {
    // 处理音视频文件，解码成本地文件
    get_info(argv[1]);
    player_decode(argv[1]);

    // 使用SDL2库进行播放
    yuv_player(argv[1]);
    pcm_player(argv[1]);
    return 0;
}
