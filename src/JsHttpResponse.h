// JsHttpResponse.h

#include <napi.h>
#include <queue>
#include <mutex>

class JsHttpResponse {
public:
    JsHttpResponse(
        Napi::Env env,
        const Napi::Function& writeFunc,
        const Napi::Function& endFunc,
        const Napi::Function& errorFunc
    );
    ~JsHttpResponse();

    void write(const uint8_t* data, size_t size);
    void end();
    void error(const std::string& msg);

private:
    Napi::ThreadSafeFunction writeFn;
    Napi::ThreadSafeFunction endFn;
    Napi::ThreadSafeFunction errorFn;
};
