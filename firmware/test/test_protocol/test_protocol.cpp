#include <unity.h>
#include <string.h>
#include "drivers/Protocol.h"
#include "drivers/Crc.h"

using namespace iot;

// ---- TX framing -------------------------------------------------------------

void test_pi30_tx_frame_has_crc_and_cr()
{
    TxFrame f;
    TEST_ASSERT_TRUE(frameCommand(ProtocolKind::PI30, true, false, "QPIGS", f));
    TEST_ASSERT_EQUAL_UINT16(8, f.len);  // 5 + 2 CRC + CR
    TEST_ASSERT_EQUAL_MEMORY("QPIGS", f.buf, 5);
    TEST_ASSERT_EQUAL_HEX8(0xB7, f.buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0xA9, f.buf[6]);
    TEST_ASSERT_EQUAL_HEX8('\r', f.buf[7]);
}

void test_pi17_query_frame_no_crc()
{
    TxFrame f;
    TEST_ASSERT_TRUE(frameCommand(ProtocolKind::PI17, false, false, "GS", f));
    // ^P003GS\r — length counts cmd + CR, no CRC bytes
    TEST_ASSERT_EQUAL_UINT16(8, f.len);
    TEST_ASSERT_EQUAL_MEMORY("^P003GS\r", f.buf, 8);
}

void test_pi17_setter_frame_uses_S_prefix()
{
    TxFrame f;
    TEST_ASSERT_TRUE(frameCommand(ProtocolKind::PI17, false, true, "GPMP015000", f));
    TEST_ASSERT_EQUAL_MEMORY("^S011GPMP015000\r", f.buf, 16);
    TEST_ASSERT_EQUAL_UINT16(16, f.len);
}

// ---- RX deframing -----------------------------------------------------------

static uint16_t withCrc(char* buf, const char* body)
{
    size_t n = strlen(body);
    memcpy(buf, body, n);
    uint16_t crc = mppCrc((const uint8_t*)body, n);
    buf[n] = (char)(crc >> 8);
    buf[n + 1] = (char)(crc & 0xFF);
    return (uint16_t)(n + 2);
}

void test_pi30_response_data()
{
    char line[128];
    uint16_t len = withCrc(line, "(230.0 50.0 230.0 50.0 0435 0300 003 460 57.50");
    RxView v;
    TEST_ASSERT_EQUAL(int(RespStatus::Data), int(parseResponse(ProtocolKind::PI30, line, len, v)));
    TEST_ASSERT_EQUAL_MEMORY("230.0", v.payload, 5);
    TEST_ASSERT_EQUAL_UINT16(len - 3, v.len);
}

void test_pi30_ack_nak()
{
    char line[16];
    RxView v;
    uint16_t len = withCrc(line, "(ACK");
    TEST_ASSERT_EQUAL(int(RespStatus::Ack), int(parseResponse(ProtocolKind::PI30, line, len, v)));
    len = withCrc(line, "(NAK");
    TEST_ASSERT_EQUAL(int(RespStatus::Nak), int(parseResponse(ProtocolKind::PI30, line, len, v)));
}

void test_pi30_bad_crc_rejected()
{
    char line[64];
    uint16_t len = withCrc(line, "(230.0 50.0");
    line[2] = '9';  // corrupt a payload byte after CRC computed
    RxView v;
    TEST_ASSERT_EQUAL(int(RespStatus::BadCrc), int(parseResponse(ProtocolKind::PI30, line, len, v)));
}

void test_pi17_data_response()
{
    // Real captured frame: ^D00517 + CRC(0xCAEC): payload "17"
    char line[16];
    uint16_t len = withCrc(line, "^D00517");
    RxView v;
    TEST_ASSERT_EQUAL(int(RespStatus::Data), int(parseResponse(ProtocolKind::PI17, line, len, v)));
    TEST_ASSERT_EQUAL_UINT16(2, v.len);
    TEST_ASSERT_EQUAL_MEMORY("17", v.payload, 2);
}

void test_pi17_ack_nak()
{
    char line[8];
    RxView v;
    uint16_t len = withCrc(line, "^1");
    TEST_ASSERT_EQUAL(int(RespStatus::Ack), int(parseResponse(ProtocolKind::PI17, line, len, v)));
    len = withCrc(line, "^0");
    TEST_ASSERT_EQUAL(int(RespStatus::Nak), int(parseResponse(ProtocolKind::PI17, line, len, v)));
}

void test_malformed_rejected()
{
    char line[32];
    uint16_t len = withCrc(line, "garbage");
    RxView v;
    TEST_ASSERT_EQUAL(int(RespStatus::Malformed), int(parseResponse(ProtocolKind::PI30, line, len, v)));
    TEST_ASSERT_EQUAL(int(RespStatus::Malformed), int(parseResponse(ProtocolKind::PI17, line, len, v)));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_pi30_tx_frame_has_crc_and_cr);
    RUN_TEST(test_pi17_query_frame_no_crc);
    RUN_TEST(test_pi17_setter_frame_uses_S_prefix);
    RUN_TEST(test_pi30_response_data);
    RUN_TEST(test_pi30_ack_nak);
    RUN_TEST(test_pi30_bad_crc_rejected);
    RUN_TEST(test_pi17_data_response);
    RUN_TEST(test_pi17_ack_nak);
    RUN_TEST(test_malformed_rejected);
    return UNITY_END();
}
