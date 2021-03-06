// SDL_play_yuv.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <stdio.h>
#pragma warning(disable : 4996)

extern "C"
{
#include "SDL2/SDL.h"
}

const int bpp = 12;
//screan 为屏幕长宽，pixel为视频长宽
int screen_w = 640, screen_h = 272;
const int pixel_w = 640, pixel_h = 272;

unsigned char buffer[pixel_w*pixel_h*bpp / 8];

//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//Break
#define BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit = 0;

int refresh_video(void *opaque) {
	thread_exit = 0;
	while (thread_exit == 0) {
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);  //发送一个事件
		SDL_Delay(40);  //工具函数，可用于调节播放速度
	}
	thread_exit = 0;
	//Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}

int main(int argc, char* argv[])
{
	//初始化 SDL 系统
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	SDL_Window *screen;
	//SDL 2.0 Support for multiple windows
	//创建窗口 SDL_Window
	screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}
	//创建渲染器SDL_Renderer
	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

	Uint32 pixformat = 0;
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	pixformat = SDL_PIXELFORMAT_IYUV;
	//创建纹理 SDL_Texture
	SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STREAMING, pixel_w, pixel_h);

	FILE *fp = NULL;
	//打开yuv文件
	fp = fopen("output.yuv", "rb+");

	if (fp == NULL) {
		printf("cannot open this file\n");
		return -1;
	}

	//SDL_Rect srcRect[4];
	//SDL_Rect sdlRect[4];
	SDL_Rect sdlRect;
	//创建一个线程
	SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video, NULL, NULL);
	SDL_Event event;
	while (1) {
		//等待一个事件
		SDL_WaitEvent(&event);
		if (event.type == REFRESH_EVENT) {
			if (fread(buffer, 1, pixel_w*pixel_h*bpp / 8, fp) != pixel_w * pixel_h*bpp / 8) {
				// Loop
				fseek(fp, 0, SEEK_SET);
				fread(buffer, 1, pixel_w*pixel_h*bpp / 8, fp);
			}
			//设置纹理的数据
			SDL_UpdateTexture(sdlTexture, NULL, buffer, pixel_w);
#if 0 //分屏播放
			srcRect[0].x = 0;
			srcRect[0].y = 0;
			srcRect[0].w = 320;
			srcRect[0].h = 176;

			srcRect[1].x = 320;
			srcRect[1].y = 0;
			srcRect[1].w = 320;
			srcRect[1].h = 176;

			srcRect[2].x = 0;
			srcRect[2].y = 176;
			srcRect[2].w = 320;
			srcRect[2].h = 176;

			srcRect[3].x = 320;
			srcRect[3].y = 176;
			srcRect[3].w = 320;
			srcRect[3].h = 176;
			//FIX: If window is resize
			sdlRect[0].x = 0;
			sdlRect[0].y = 0;
			sdlRect[0].w = 320;
			sdlRect[0].h = 176;

			sdlRect[1].x = 330;
			sdlRect[1].y = 0;
			sdlRect[1].w = 320;
			sdlRect[1].h = 176;

			sdlRect[2].x = 0;
			sdlRect[2].y = 186;
			sdlRect[2].w = 320;
			sdlRect[2].h = 176;

			sdlRect[3].x = 330;
			sdlRect[3].y = 186;
			sdlRect[3].w = 320;
			sdlRect[3].h = 176;
			//清空纹理
			SDL_RenderClear(sdlRenderer);
			//将纹理的数据拷贝给渲染器
			SDL_RenderCopy(sdlRenderer, sdlTexture,/*NULL*/ &srcRect[0], &sdlRect[0]);
			SDL_RenderCopy(sdlRenderer, sdlTexture,/*NULL*/ &srcRect[1], &sdlRect[1]);
			SDL_RenderCopy(sdlRenderer, sdlTexture,/*NULL*/ &srcRect[2], &sdlRect[2]);
			SDL_RenderCopy(sdlRenderer, sdlTexture,/*NULL*/ &srcRect[3], &sdlRect[3]);
#endif

			sdlRect.x = 0;
			sdlRect.y = 0;
			sdlRect.w = screen_w;
			sdlRect.h = screen_h;

			SDL_RenderClear(sdlRenderer);
			SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);

			//显示
			SDL_RenderPresent(sdlRenderer);

		}
		else if (event.type == SDL_WINDOWEVENT) {
			//If Resize
			SDL_GetWindowSize(screen, &screen_w, &screen_h);
		}
		else if (event.type == SDL_QUIT) {
			thread_exit = 1;
		}
		else if (event.type == BREAK_EVENT) {
			break;
		}
	}
	//退出 SDL 系统
	SDL_Quit();
	return 0;
}
