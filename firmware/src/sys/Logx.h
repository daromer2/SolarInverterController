#pragma once
#include <Arduino.h>

// Minimal leveled logger: USB serial + RAM ring buffer served at /api/log.
namespace logx {

void begin();
void log(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// Copies the ring buffer (oldest first) into out; returns bytes written.
size_t dump(char* out, size_t maxLen);

} // namespace logx

#define LOGI(tag, ...) logx::log(tag, __VA_ARGS__)
