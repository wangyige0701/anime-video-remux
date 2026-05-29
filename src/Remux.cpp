// Remux.cpp

#include <string>
#include <iostream>
#include <stdexcept>

#include <Remux.h>

Remux::Remux(const std::string& path, JsHttpResponse* response)
    : inputPath(path), response(response) {
}

Remux::~Remux() {
    cleanup();
    response.reset();
}

void Remux::open(double seekSeconds) {
    openInput();
    openOutput();

    if (seekSeconds >= 0) {
        seek(seekSeconds);
    }

    writeHeader();
}

void Remux::stop() {
    stopped.store(true);
}

void Remux::stream() {
    AVPacket* pkt = av_packet_alloc();

    if (!pkt) {
        return;
    }

    while (!stopped && av_read_frame(ifmt, pkt) >= 0) {
        AVStream* in = ifmt->streams[pkt->stream_index];

        if (pkt->stream_index >= ofmt->nb_streams) {
            av_packet_unref(pkt);
            continue;
        }

        AVStream* out = ofmt->streams[pkt->stream_index];

        pkt->pts = av_rescale_q_rnd(pkt->pts, in->time_base, out->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, in->time_base, out->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, in->time_base, out->time_base);

        if (pkt->duration <= 0) {
            if (in->avg_frame_rate.num > 0) {
                pkt->duration = av_rescale_q(1, av_inv_q(in->avg_frame_rate), out->time_base);
            } else {
                pkt->duration = 1;
            }
        }

        pkt->pos = -1;

        av_interleaved_write_frame(ofmt, pkt);
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_write_trailer(ofmt);
    response->end();
}

double Remux::getDuration(const std::string& path) {
    AVFormatContext* fmt = nullptr;

    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) {
        return -1;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return -1;
    }

    double duration = -1;
    if (fmt->duration != AV_NOPTS_VALUE) {
        duration = (double) fmt->duration / AV_TIME_BASE;
    }

    avformat_close_input(&fmt);
    return duration;
}

void Remux::openInput() {
    ifmt = avformat_alloc_context();
    ifmt->interrupt_callback.callback = &Remux::interruptCallback;
    ifmt->interrupt_callback.opaque = this;

    if (avformat_open_input(&ifmt, inputPath.c_str(), nullptr, nullptr) < 0) {
        throw std::runtime_error("Failed to open input file");
    }

    if (avformat_find_stream_info(ifmt, nullptr) < 0) {
        throw std::runtime_error("Failed to find stream info");
    }
}

void Remux::openOutput() {
    avformat_alloc_output_context2(&ofmt, nullptr, "mp4", nullptr);
    if (!ofmt) {
        throw std::runtime_error("Failed to allocate output context");
    }

    // 复制流
    for (unsigned i = 0; i < ifmt->nb_streams; i++) {
        AVStream* in = ifmt->streams[i];

        if (in->codecpar->codec_type != AVMEDIA_TYPE_VIDEO && in->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }

        // EAC3 不进行转封装
        if (in->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && in->codecpar->codec_id == AV_CODEC_ID_EAC3) {
            continue;
        }

        AVStream* out = avformat_new_stream(ofmt, nullptr);
        out->codecpar->extradata = nullptr;
        out->codecpar->extradata_size = 0;
        avcodec_parameters_copy(out->codecpar, in->codecpar);
        out->codecpar->codec_tag = 0;
        out->time_base = in->time_base;
    }

    // 自定义 IO
    avioBuffer = (uint8_t*) av_malloc(32768);
    avio = avio_alloc_context(avioBuffer, 32768, 1, this, nullptr, &Remux::writePacket, nullptr);

    ofmt->pb = avio;
    ofmt->flags |= AVFMT_FLAG_CUSTOM_IO;
}

void Remux::writeHeader() {
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);

    if (avformat_write_header(ofmt, &opts) < 0) {
        throw std::runtime_error("Failed to write header");
    }

    av_dict_free(&opts);
}

void Remux::seek(double seconds) {
    int64_t ts = seconds * AV_TIME_BASE;
    av_seek_frame(ifmt, -1, ts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
    avformat_flush(ifmt);
}

void Remux::cleanup() {
    if (ifmt) {
        avformat_close_input(&ifmt);
    }

    if (ofmt) {
        avformat_free_context(ofmt);
    }

    if (avio) {
        avio_context_free(&avio);
    }

    avioBuffer = nullptr;
}

int Remux::writePacket(void* opaque, const uint8_t* buf, int size) {
    Remux* self = static_cast<Remux*>(opaque);
    if (self->stopped) {
        return AVERROR_EOF;
    }
    self->response->write(buf, size);
    return size;
}

int Remux::interruptCallback(void* opaque) {
    Remux* self = static_cast<Remux*>(opaque);
    return self->stopped.load() ? 1 : 0;
}
