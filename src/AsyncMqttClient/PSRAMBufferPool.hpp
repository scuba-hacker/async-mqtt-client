#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cstdlib>

// Feature flag to enable/disable PSRAM buffer pool
// Set to 1 to enable PSRAM buffering, 0 to use original std::vector behavior
// Can be overridden by defining ASYNCMQTT_USE_PSRAM_BUFFER=0 in your build flags
#ifndef ASYNCMQTT_USE_PSRAM_BUFFER
#define ASYNCMQTT_USE_PSRAM_BUFFER 1  // Enable PSRAM buffering with lazy initialization
#endif

class PSRAMBufferPool {
private:
    static uint8_t* _psramBuffer;
    static size_t _bufferSize;
    static size_t _nextOffset;
    static bool _initialized;

public:
    static const int default_size = 1024 * 512; // 512KB default - conservative size for testing
    static void init(size_t size = default_size);  // 512KB default
    static uint8_t* allocate(size_t size);
    static void cleanup();
    static bool isInitialized() { return _initialized; }

    // Buffer utilization monitoring
    static size_t getCurrentOffset() { return _nextOffset; }
    static size_t getBufferSize() { return _bufferSize; }
    static float getUtilizationPercent() {
        return _bufferSize > 0 ? (float)_nextOffset * 100.0f / _bufferSize : 0.0f;
    }
    static size_t getBytesUsed() { return _nextOffset; }
    static size_t getBytesAvailable() { return _bufferSize > _nextOffset ? _bufferSize - _nextOffset : 0; }
};