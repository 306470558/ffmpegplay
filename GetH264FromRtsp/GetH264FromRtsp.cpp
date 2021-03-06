// GetH264FromRtsp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>

#include <stdio.h>
#pragma warning(disable : 4996)
 
#define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#define AVFMT_RAWPICTURE 0x0020

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
};
#endif
#endif

int main(int argc, char **argv)
{
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename, *out_filename;
	int ret, i;
	int video_index = -1;
	int frame_index = 0;
	in_filename = "rtsp://192.168.15.3:8086/";
	out_filename = "receive.h264";

	av_register_all();
	avformat_network_init();
	//打开输入流	
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0)
	{
		printf("Could not open input file.\n");
		system("pause");
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
	{
		printf("Failed to retrieve input stream information\n");
		goto end;
	}

	//nb_streams代表有几路流，一般是2路：即音频和视频，顺序不一定
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {

		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			//这一路是视频流，标记一下，以后取视频流都从ifmt_ctx->streams[video_index]取
			video_index = i;
			break;
		}
	}

	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	//打开输出流
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);

	if (!ofmt_ctx)
	{
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{    //根据输入流创建输出流
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream)
		{
			printf("Failed allocating output stream.\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		//将输出流的编码信息复制到输入流
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0)
		{
			printf("Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;

		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	}

	//Dump format--------------------
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	//打开输出文件
	if (!(ofmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			printf("Could not open output URL '%s'", out_filename);
			goto end;
		}
	}
	//system("pause");
	//写文件头到输出文件
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0)
	{
		printf("Error occured when opening output URL\n");
		goto end;
	}

	//while循环中持续获取数据包，不管音频视频都存入文件
	while (1)
	{
		AVStream *in_stream, *out_stream;
		//从输入流获取一个数据包
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;

		in_stream = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		//copy packet
		//转换 PTS/DTS 时序
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		//printf("pts %d dts %d base %d\n",pkt.pts,pkt.dts, in_stream->time_base);
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;

		printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
		//此while循环中并非所有packet都是视频帧，当收到视频帧时记录一下，仅此而已
		if (pkt.stream_index == video_index)
		{
			printf("Receive %8d video frames from input URL\n", frame_index);
			frame_index++;
		}

		//将包数据写入到文件。
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0)
		{
			/*
			当网络有问题时，容易出现到达包的先后不一致，pts时序混乱会导致
			av_interleaved_write_frame函数报 -22 错误。暂时先丢弃这些迟来的帧吧
			若所大部分包都没有pts时序，那就要看情况自己补上时序（比如较前一帧时序+1）再写入。
			*/
			if (ret == -22) {
				continue;
			}
			else {
				printf("Error muxing packet.error code %d\n", ret);
				break;
			}
		}

		//av_free_packet(&pkt); //此句在新版本中已deprecated 由av_packet_unref代替
		av_packet_unref(&pkt);

	}

	//写文件尾
	av_write_trailer(ofmt_ctx);
	
end:
	avformat_close_input(&ifmt_ctx);
	//Close input
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF)
	{
		printf("Error occured.\n");
		return -1;
	}

	
	return 0;

}
