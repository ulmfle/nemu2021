#include "cpu/exec/helper.h"

#if DATA_BYTE == 1

#define SUFFIX b
#define DATA_TYPE uint8_t
#define DATA_TYPE_S int8_t

#elif DATA_BYTE == 2

#define SUFFIX w
#define DATA_TYPE uint16_t
#define DATA_TYPE_S int16_t

#elif DATA_BYTE == 4

#define SUFFIX l
#define DATA_TYPE uint32_t
#define DATA_TYPE_S int32_t

#else

#error unknown DATA_BYTE

#endif

#define REG(index) concat(reg_, SUFFIX) (index)
#define REG_NAME(index) concat(regs, SUFFIX) [index]

#define MEM_R(_sr, addr) swaddr_read(addr, DATA_BYTE, _sr)
#define MEM_W(_sr, addr, data) swaddr_write(addr, DATA_BYTE, _sr, data)

#define OPERAND_W(op, src) concat(write_operand_, SUFFIX) (op, src)

#define MSB(n) ((DATA_TYPE)(n) >> ((DATA_BYTE << 3) - 1))

// add for convenience
#define PUSH(data) do { cpu.esp -= DATA_BYTE; MEM_W(SR_SS, cpu.esp, data); } while (0)

#define POP(dest) do { dest = MEM_R(SR_SS, cpu.esp); cpu.esp += DATA_BYTE; } while (0)