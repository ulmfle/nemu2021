#include "common.h"
#include "cpu/decode/decode.h"
hwaddr_t page_translate(lnaddr_t);
/* shared by all helper function */
Operands ops_decoded;

#define DATA_BYTE 1
#include "decode-template.h"
#undef DATA_BYTE

#define DATA_BYTE 2
#include "decode-template.h"
#undef DATA_BYTE

#define DATA_BYTE 4
#include "decode-template.h"
#undef DATA_BYTE
