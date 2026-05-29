// Remux.h

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <string>
#include <atomic>
#include <JsHttpResponse.h>

class Remux {
public:
    Remux(const std::string& path, JsHttpResponse* response);
    ~Remux();

    void open(double seekSeconds = -1.0);
    void stop();

    void stream();
    /**
     * 返回输入文件的时长，单位秒
     */
    static double getDuration(const std::string& path);

private:
    std::atomic<bool> stopped{ false };
    std::unique_ptr<JsHttpResponse> response;

    std::string inputPath;

    AVFormatContext* ifmt = nullptr;
    AVFormatContext* ofmt = nullptr;
    AVIOContext* avio = nullptr;
    uint8_t* avioBuffer = nullptr;

    void openInput();
    void openOutput();
    void writeHeader();
    void seek(double seconds);
    void cleanup();

    /**
     * AVIO 写回调
     */
    static int writePacket(void* opaque, const uint8_t* buf, int size);
    static int interruptCallback(void* opaque);
};
