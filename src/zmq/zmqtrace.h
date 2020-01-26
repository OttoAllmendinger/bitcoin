#ifndef BITCOIN_ZMQ_TRACE_H
#define BITCOIN_ZMQ_TRACE_H

#include <string>
#include <vector>
#include <zmq/zmqconfig.h>
#include <logging.h>

#include <uint256.h>

static void* pSocketTrace = nullptr;

static void* GetTraceSocket() {
    LogPrint(BCLog::ZMQ, "ZTRACE: GetTraceSocket()\n");

    if (pSocketTrace) {
        LogPrint(BCLog::ZMQ, "ZTRACE: returning cached pSocketTrace\n");
        return pSocketTrace;
    }

    const char *address = std::getenv("ZTRACE_ADDRESS");
    if (!address) {
        LogPrint(BCLog::ZMQ, "ZTRACE: ZTRACE_ADDRESS not set\n");
        return nullptr;
    }

    LogPrint(BCLog::ZMQ, "ZTRACE: using address %s\n", address);

    void* pcontext = zmq_ctx_new();
    if (!pcontext) {
        zmqError("ZTRACE: Unable to initialize context");
        return nullptr;
    }
    void * psocket = zmq_socket(pcontext, ZMQ_PUB);
    if (!psocket) {
        zmqError("ZTRACE: Failed to create socket");
        return nullptr;
    }

    int highWaterMark = 1000;
    int rc = zmq_setsockopt(psocket, ZMQ_SNDHWM, &highWaterMark, sizeof(highWaterMark));
    if (rc != 0) {
        zmqError("ZTRACE: Failed to set outbound message high water mark");
        zmq_close(psocket);
        return nullptr;
    }

    rc = zmq_bind(psocket, address);
    if (rc != 0) {
        zmqError("ZTRACE: Failed to bind address");
        zmq_close(psocket);
        return nullptr;
    }

    pSocketTrace = psocket;

    return pSocketTrace;
}


class ZTrace
{
    std::vector<std::vector<uint8_t>> parts = {};

public:
    ZTrace(std::string m)
    {
        add("ztrace");
        add(m);
    }

    ZTrace& add(std::string part)
    {
        return add(std::vector<uint8_t>(part.begin(), part.end()));
    }

    ZTrace& add(std::vector<uint8_t> part)
    {
        parts.push_back(part);
        return *this;
    }

    ZTrace& add(void* src, size_t size)
    {
        unsigned char* charBuf = (unsigned char*)src;
        std::vector<uint8_t> part(charBuf, charBuf + size);
        return add(part);
    }

    ZTrace& add(uint256 data) {
        std::vector<uint8_t> part(data.begin(), data.end());
        return add(part);
    }

    void send()
    {
        void *sock = GetTraceSocket();
        if (!sock) {
            LogPrint(BCLog::ZMQ, "ZTRACE: error: no sock\n");
            return;
        }

        for (size_t i = 0; i < parts.size(); i++) {
            auto const& part = parts[i];

            zmq_msg_t msg;

            int rc = zmq_msg_init_size(&msg, part.size());
            if (rc != 0) {
                zmqError("ZTRACE: Unable to initialize ZMQ msg");
                return;
            }

            void* buf = zmq_msg_data(&msg);

            std::memcpy(buf, part.data(), part.size());

            rc = zmq_msg_send(&msg, sock, (i < (parts.size() - 1)) ? ZMQ_SNDMORE : 0);
            if (rc == -1) {
                zmqError("ZTRACE: Unable to send ZMQ msg");
                zmq_msg_close(&msg);
                return;
            }

            zmq_msg_close(&msg);
        }

        LogPrint(BCLog::ZMQ, "sent message with %d parts\n", parts.size());
    }
};

#endif