#include "prefix/prefix.h"

#include "data-mov/mov.h"
#include "data-mov/xchg.h"
#include "data-mov/movext.h"
#include "data-mov/cmovcc.h"
#include "data-mov/cltd.h"
#include "data-mov/push.h"
#include "data-mov/pusha.h"
#include "data-mov/pop.h"
#include "data-mov/popa.h"
#include "data-mov/enter.h"
#include "data-mov/leave.h"
#include "data-mov/in.h"
#include "data-mov/out.h"

#include "bit/bt.h"

#include "arith/adc.h"
#include "arith/add.h"
#include "arith/dec.h"
#include "arith/inc.h"
#include "arith/neg.h"
#include "arith/imul.h"
#include "arith/mul.h"
#include "arith/idiv.h"
#include "arith/div.h"
#include "arith/sbb.h"
#include "arith/sub.h"
#include "arith/cmp.h"

#include "control/jmp.h"
#include "control/jcc.h"
#include "control/call.h"
#include "control/ret.h"

#include "logic/and.h"
#include "logic/or.h"
#include "logic/not.h"
#include "logic/xor.h"
#include "logic/sar.h"
#include "logic/shl.h"
#include "logic/shr.h"
#include "logic/shrd.h"
#include "logic/shld.h"
#include "logic/test.h"
#include "logic/setcc.h"

#include "string/rep.h"
#include "string/scas.h"
#include "string/stos.h"
#include "string/movs.h"
#include "string/lods.h"

#include "sreg/ldt.h"

#include "intr/int.h"
#include "intr/iret.h"
#include "intr/hlt.h"

#include "misc/misc.h"

#include "special/special.h"

