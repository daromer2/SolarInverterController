#include <unity.h>
#include <cstring>
#include <cstdio>
#include "drivers/Crc.h"

using namespace iot;

static uint16_t crcOf(const char* s)
{
    return mppCrc((const uint8_t*)s, strlen(s));
}

// Vectors verified against real captured inverter frames (jblance/mpp-solar
// test_responses) and the documented PI30 QPIGS CRC.
void test_pi30_command_vectors()
{
    TEST_ASSERT_EQUAL_HEX16(0xB7A9, crcOf("QPIGS"));
    TEST_ASSERT_EQUAL_HEX16(0x49C1, crcOf("QMOD"));
    TEST_ASSERT_EQUAL_HEX16(0xB4DA, crcOf("QPIWS"));
    TEST_ASSERT_EQUAL_HEX16(0xD6EA, crcOf("QID"));
    TEST_ASSERT_EQUAL_HEX16(0xE20B, crcOf("POP02"));
}

void test_pi17_response_vectors()
{
    // Captured: b"^D00517\xca\xec\r", b"^D00505\xd9\x9f\r", b"^D00815000\xe1\xa1\r"
    TEST_ASSERT_EQUAL_HEX16(0xCAEC, crcOf("^D00517"));
    TEST_ASSERT_EQUAL_HEX16(0xD99F, crcOf("^D00505"));
    TEST_ASSERT_EQUAL_HEX16(0xE1A1, crcOf("^D00815000"));
    // Setter NAK frame: b"^0\x1b\xe3\r"
    TEST_ASSERT_EQUAL_HEX16(0x1BE3, crcOf("^0"));
    TEST_ASSERT_EQUAL_HEX16(0x0BC2, crcOf("^1"));
}

void test_byte_bump_never_emits_framing_bytes()
{
    // Exhaustively-ish scan: no CRC byte may equal '(' CR or LF
    char buf[8];
    for (int i = 0; i < 5000; ++i) {
        snprintf(buf, sizeof(buf), "T%04d", i);
        uint16_t c = crcOf(buf);
        uint8_t hi = c >> 8, lo = c & 0xFF;
        TEST_ASSERT_TRUE(hi != 0x28 && hi != 0x0d && hi != 0x0a);
        TEST_ASSERT_TRUE(lo != 0x28 && lo != 0x0d && lo != 0x0a);
    }
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_pi30_command_vectors);
    RUN_TEST(test_pi17_response_vectors);
    RUN_TEST(test_byte_bump_never_emits_framing_bytes);
    return UNITY_END();
}
