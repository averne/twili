#pragma once

#define TWILI_RESULT(code) (((code) << 9) | 0xEF)

#define TWILI_ERR_INVALID_NRO TWILI_RESULT(1)
#define TWILI_ERR_NO_CRASHED_PROCESSES TWILI_RESULT(2)
#define TWILI_ERR_RESPONSE_SIZE_MISMATCH TWILI_RESULT(3)
#define TWILI_ERR_FATAL_USB_TRANSFER TWILI_RESULT(4)
#define TWILI_ERR_USB_TRANSFER TWILI_RESULT(5)
#define TWILI_ERR_UNRECOGNIZED_PID TWILI_RESULT(7)
#define TWILI_ERR_UNRECOGNIZED_HANDLE_PLACEHOLDER TWILI_RESULT(8)
#define TWILI_ERR_INVALID_SEGMENT TWILI_RESULT(9)
#define TWILI_ERR_IO_ERROR TWILI_RESULT(10)
#define TWILI_ERR_WONT_DEBUG_SELF TWILI_RESULT(11)
#define TWILI_ERR_INVALID_PIPE TWILI_RESULT(12)
#define TWILI_ERR_EOF TWILI_RESULT(13)
#define TWILI_ERR_INVALID_PIPE_STATE TWILI_RESULT(14)
#define TWILI_ERR_PIPE_ALREADY_EXISTS TWILI_RESULT(15)
#define TWILI_ERR_NO_SUCH_PIPE TWILI_RESULT(16)
#define TWILI_ERR_BAD_RESPONSE TWILI_RESULT(17)
#define TWILI_ERR_ALREADY_WAITING TWILI_RESULT(18)
#define TWILI_ERR_FATAL_BRIDGE_STATE TWILI_RESULT(19)
#define TWILI_ERR_TCP_TRANSFER TWILI_RESULT(20)

#define TWILI_ERR_PROTOCOL_UNRECOGNIZED_OBJECT TWILI_RESULT(1001)
#define TWILI_ERR_PROTOCOL_UNRECOGNIZED_FUNCTION TWILI_RESULT(1002)
#define TWILI_ERR_PROTOCOL_TRANSFER_ERROR TWILI_RESULT(1003)
#define TWILI_ERR_PROTOCOL_UNRECOGNIZED_DEVICE TWILI_RESULT(1004)
#define TWILI_ERR_PROTOCOL_BAD_REQUEST TWILI_RESULT(1005)
#define TWILI_ERR_BAD_REQUEST TWILI_ERR_PROTOCOL_BAD_REQUEST // old alias
