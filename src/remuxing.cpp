/*
 * Copyright (c) 2013 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * @file
 * libavformat/libavcodec demuxing and muxing API example.
 *
 * Remux streams from one container format to another.
 * @example remuxing.c
 */

extern "C" {
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>    
#include "log.h"
}

bool checkArguments(int argc, char** argv)
{
    // �Ķ���� �˻�
    if (argc < 3) {
        printf("usage: %s input output\n"
            "API example program to remux a media file with libavformat and libavcodec.\n"
            "The output format is guessed according to the file extension.\n"
            "\n", argv[0]);
        return false;
    }
    return true;
}

struct FormatContextOpenResult
{
    AVFormatContext* fmContext;
    bool success;
};

FormatContextOpenResult openFormatContextForInput(const char* inputName)
{
    int ret;
    AVFormatContext* fmContext = nullptr;

    // �Է� ��Ʈ�� ���� �� ��� �б� 
    if ((ret = avformat_open_input(&fmContext, inputName, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", inputName);
        return { fmContext, false };
    }
    // ��Ŷ�� �о� ��Ʈ�� ������ ���ϱ� - ����� ���� ������ ���� �� ������.
    if ((ret = avformat_find_stream_info(fmContext, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        return { fmContext, false };
    }
    // ��Ʈ�� ���� ���
    av_dump_format(fmContext, 0, inputName, 0);
    return { fmContext, true };
}

FormatContextOpenResult openFormatContextForOutput(const char* outputName)
{
    int ret;
    AVFormatContext* fmtContext = NULL;

    // ��¿� AVFormatContext �Ҵ�. ���� �ÿ��� avformat_free_context() �� ȣ��.
    avformat_alloc_output_context2(&fmtContext, NULL, NULL, outputName);
    if (!fmtContext) {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return { fmtContext, false };
    }
    return { fmtContext, true };
}

struct RemuxMapResult {
    int *map;
    int ret;
};

RemuxMapResult createRemuxingMappingTable(int streamCount)
{
    int ret = 0;
    int *stream_mapping = static_cast<int*>(av_mallocz_array(streamCount, sizeof(*stream_mapping)));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
    }
    return { stream_mapping, ret};
}

int createRemuxingOutStream(AVFormatContext* ifmt_ctx, AVFormatContext* ofmt_ctx, int *stream_mapping)
{
    int stream_index = 0;
    int ret = 0;

    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        // ifmt_ctx->streams �� �Է� ���� ���� ��Ʈ�� ����Ʈ��.
        AVStream *in_stream = ifmt_ctx->streams[i];
        // ��Ʈ���� ���� �ڵ� �Ķ������.
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        // remuxing�� audio, video, subtitle�� ���ؼ� ��.
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }
        // stream_mapping �� index�� �Է��� stream index, value�� ����� stream_index, -1�� ���� �������� ����.
        stream_mapping[i] = stream_index++;
        // ��� ���Ͽ� ��Ʈ���� ����. remuxing �� ��Ʈ�� ������ŭ �����ϰ� ��.
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            //goto end;
            return ret;
        }
        // remuxing�� transcoding�� ���� �ʰ� �״�� �����͸� �����ϹǷ�, �ڵ� �Ķ���͵� ������.
        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            //goto end;
            return ret;
        }
        // ���������Ƿ� codec_tag�� �Է� ������ ���� ���� �����ٵ� 0���� ����� ������?
        out_stream->codecpar->codec_tag = 0;
    }
    return 0;
}

auto openAvioForOutput(AVFormatContext* ofmt_ctx, const char* out_filename) {
    int ret = 0;
    // ��� �����̳� ������ ������
    AVOutputFormat *ofmt = ofmt_ctx->oformat;
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        // out_filename���� ������ �� �ִ� AVIOContext ���� �� �ʱ�ȭ
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", out_filename);
            //goto end;
            return ret;
        }
    }
    return ret;
};

auto writeFrameRemuxing(AVFormatContext* ifmt_ctx, int *stream_mapping, int stream_mapping_size, AVFormatContext* ofmt_ctx) {
    AVPacket pkt;
    int ret = 0;
    while (1) {
        AVStream *in_stream, *out_stream;
        // ��Ʈ���� ���� �������� �о���δ�.
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        in_stream = ifmt_ctx->streams[pkt.stream_index];
        // ������ �ʴ� �������̶�� �����Ѵ�.
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }
        // ��� �ʿ� ���Ͽ� ���� ��Ʈ�� index�� ��Ʈ�� ������ �����.
        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        log_packet(ifmt_ctx, &pkt, "in");
        /* copy packet */
        // �Է� ��Ʈ���� ��� ��Ʈ���� ������ ���� �ð����� ���.
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        log_packet(ofmt_ctx, &pkt, "out");
        // dts ������� interleaving ������� �̵�� ����
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }
    return ret;
};

auto closeAvioForOutput(AVFormatContext* ofmt_ctx)  {
    /* close output */
    // demuxer�� ����ϰ� �Ǹ� avio_open�� ȣ���ϸ�, �� �� AVFMT_NOFILE �� ������.
    AVOutputFormat *ofmt = ofmt_ctx->oformat;
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) {
        // avio_open���� ������ ��쿡�� ����ؾ� ��.
        avio_closep(&ofmt_ctx->pb);
    }
};

int main(int argc, char **argv)
{
    // ���� �ʱ�ȭ
    //AVOutputFormat *ofmt = NULL;

        
    int ret;

    if (!checkArguments(argc, argv)) {
        return 1;
    }
        
    // �Է� ���� �̸� ���� 
    const char* in_filename = argv[1];        
    auto inResult = openFormatContextForInput(in_filename);
    AVFormatContext* ifmt_ctx = inResult.fmContext;
    if (!inResult.success) {
        goto end;
    }

    // ��� ���� �̸� ����
    const char* out_filename = argv[2];
    auto outResult = openFormatContextForOutput(out_filename);
    AVFormatContext* ofmt_ctx = outResult.fmContext;
    if (!outResult.success) {
        goto end;
    }

    // remuxing�� ���ؼ� ��Ʈ���� ���� ���� ���̺� ����

    auto remuxMapResult = createRemuxingMappingTable(ifmt_ctx->nb_streams);
    ret = remuxMapResult.ret;
    if (ret) {
        goto end;
    }
    auto stream_mapping = remuxMapResult.map;



    ret = createRemuxingOutStream(ifmt_ctx, ofmt_ctx, stream_mapping);
    if (ret < 0) {
        goto end;
    }

    //��� ���� ��Ʈ�� ���� ���
    av_dump_format(ofmt_ctx, 0, out_filename, 1);


    if (openAvioForOutput(ofmt_ctx, out_filename) < 0)   {
        goto end;
    }
    
    // ��Ʈ���� private data�� �Ҵ��ϰ� ��� ������ ����� �ۼ�
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }
    int stream_mapping_size = ifmt_ctx->nb_streams;


    ret = writeFrameRemuxing(ifmt_ctx, stream_mapping, stream_mapping_size, ofmt_ctx);
    // stream trailer �ۼ��ϰ� private data�� ���� - avformat_write_header ȣ���� ������ �ڿ� ����� �� ����.
    av_write_trailer(ofmt_ctx);
end:
    // AVFormatContext �ݱ�
    avformat_close_input(&ifmt_ctx);

    closeAvioForOutput(ofmt_ctx);
    // AVFormatContext �� stream���� ������.
    avformat_free_context(ofmt_ctx);
    // �������� �Ҵ��� stream map ����
    av_freep(&stream_mapping);
    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str_wrap(ret));
        return 1;
    }
    return 0;
}