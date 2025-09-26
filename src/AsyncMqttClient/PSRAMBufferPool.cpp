#include "PSRAMBufferPool.hpp"
#include "Arduino.h"
#ifdef ESP32
#include <esp_heap_caps.h>
#endif

// Static member definitions
uint8_t* PSRAMBufferPool::_psramBuffer = nullptr;
size_t PSRAMBufferPool::_bufferSize = 0;
size_t PSRAMBufferPool::_nextOffset = 0;
bool PSRAMBufferPool::_initialized = false;

void PSRAMBufferPool::init(size_t size) {
    if (_initialized) {
        Serial.printf("PSRAMBufferPool: Already initialized, skipping\n");
        return;
    }

    // Explicitly allocate from PSRAM - MUST succeed
    _psramBuffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

    if (!_psramBuffer) {
        Serial.printf("PSRAMBufferPool: CRITICAL ERROR - PSRAM allocation failed for %zu bytes!\n", size);
        Serial.printf("Check if PSRAM is enabled in your board configuration!\n");
        Serial.flush();
        abort();  // Crash immediately rather than continue without PSRAM
    }

    Serial.printf("PSRAMBufferPool: Successfully allocated %zu bytes in PSRAM\n", size);
    _bufferSize = size;
    _nextOffset = 0;
    _initialized = true;
}

uint8_t* PSRAMBufferPool::allocate(size_t size) {
    // Lazy initialization: initialize on first use
    if (!_initialized) {
        Serial.printf("PSRAMBufferPool: Lazy initialization on first allocate()\n");
        init();  // Use default size
    }

    if (!_psramBuffer || _bufferSize == 0) {
        Serial.printf("PSRAMBufferPool: CRITICAL ERROR - Not initialized, returning nullptr!\n");
        return nullptr;  // Return null instead of malloc to avoid leaks
    }

    // Check if packet fits at current position
    if (_nextOffset + size > _bufferSize) {
        // Packet doesn't fit, wrap to beginning
        _nextOffset = 0;
        Serial.printf("PSRAMBufferPool: Wrapping buffer, reset to offset 0\n");

        // Double-check that packet fits from beginning (should always be true if size < _bufferSize)
        if (size > _bufferSize) {
            Serial.printf("PSRAMBufferPool: CRITICAL ERROR - Packet size %zu > buffer size %zu!\n", size, _bufferSize);
            abort();  // Packet larger than entire buffer
        }
    }

    uint8_t* ptr = _psramBuffer + _nextOffset;
    _nextOffset += size;
 //   Serial.printf("PSRAMBufferPool: Allocated %zu bytes at offset %zu\n", size, _nextOffset - size);
    return ptr;
}

void PSRAMBufferPool::cleanup() {
    if (_psramBuffer) {
        heap_caps_free(_psramBuffer);  // Use heap_caps_free for PSRAM allocations
        _psramBuffer = nullptr;
    }
    _bufferSize = 0;
    _nextOffset = 0;
    _initialized = false;
}