#include "Logx.h"
#include <stdarg.h>

namespace logx {

static const size_t RING_SIZE = 4096;
static char ring[RING_SIZE];
static size_t head = 0;       // next write position
static bool wrapped = false;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void begin()
{
    Serial.begin(115200);
}

static void put(const char* s, size_t n)
{
    portENTER_CRITICAL(&mux);
    for (size_t i = 0; i < n; ++i) {
        ring[head] = s[i];
        head = (head + 1) % RING_SIZE;
        if (head == 0) wrapped = true;
    }
    portEXIT_CRITICAL(&mux);
}

void log(const char* tag, const char* fmt, ...)
{
    char line[192];
    int n = snprintf(line, sizeof(line), "[%8lu][%s] ", (unsigned long)millis(), tag);
    va_list args;
    va_start(args, fmt);
    n += vsnprintf(line + n, sizeof(line) - n - 2, fmt, args);
    va_end(args);
    if (n > (int)sizeof(line) - 2) n = sizeof(line) - 2;
    line[n++] = '\n';
    line[n] = 0;
    Serial.print(line);
    put(line, n);
}

size_t dump(char* out, size_t maxLen)
{
    portENTER_CRITICAL(&mux);
    size_t total = wrapped ? RING_SIZE : head;
    size_t start = wrapped ? head : 0;
    size_t n = total < maxLen ? total : maxLen;
    // take the newest n bytes
    size_t skip = total - n;
    for (size_t i = 0; i < n; ++i)
        out[i] = ring[(start + skip + i) % RING_SIZE];
    portEXIT_CRITICAL(&mux);
    return n;
}

} // namespace logx
