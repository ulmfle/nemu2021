#include "FLOAT.h"

FLOAT F_mul_F(FLOAT a, FLOAT b) {
	long long r = (long long)Fabs(a)*(long long)Fabs(b);
	r >>= 16;
	return (a >> 31) ^ (b >> 31) ? -((FLOAT)r) : (FLOAT)r;
}

FLOAT F_div_F(FLOAT a, FLOAT b) {
	/* Dividing two 64-bit integers needs the support of another library
	 * `libgcc', other than newlib. It is a dirty work to port `libgcc'
	 * to NEMU. In fact, it is unnecessary to perform a "64/64" division
	 * here. A "64/32" division is enough.
	 *
	 * To perform a "64/32" division, you can use the x86 instruction
	 * `div' or `idiv' by inline assembly. We provide a template for you
	 * to prevent you from uncessary details.
	 *
	 *     asm volatile ("??? %2" : "=a"(???), "=d"(???) : "r"(???), "a"(???), "d"(???));
	 *
	 * If you want to use the template above, you should fill the "???"
	 * correctly. For more information, please read the i386 manual for
	 * division instructions, and search the Internet about "inline assembly".
	 * It is OK not to use the template above, but you should figure
	 * out another way to perform the division.
	 */

	// unsigned long long a_un = (unsigned long long)Fabs(a) << 16;
	unsigned a_un = Fabs(a);
	unsigned b_un = Fabs(b);
	unsigned a_un_l = a_un >> 16, a_un_r = a_un << 16;
	FLOAT r;

	asm volatile ("div %2" : "=a"(r) : "a"(a_un_r), "r"(b_un) , "d"(a_un_l));

	return (a >> 31) ^ (b >> 31) ? -r : r;
}

FLOAT f2F(float a) {
	/* You should figure out how to convert `a' into FLOAT without
	 * introducing x87 floating point instructions. Else you can
	 * not run this code in NEMU before implementing x87 floating
	 * point instructions, which is contrary to our expectation.
	 *
	 * Hint: The bit representation of `a' is already on the
	 * stack. How do you retrieve it to another variable without
	 * performing arithmetic operations on it directly?
	 */

	int _a;
	asm volatile ("mov %1, %0" : "=a"(_a) : "r"(a));

	FLOAT of = (~0u >> 1);
	char E_pre = (_a & (0xff << 23)) >> 23;

	if (!(E_pre ^ 0xff)) return (_a >> 31) ? -of : of;

	char E = E_pre ? (E_pre - 127) : 1 - 127;
    int M = (_a & 0x7fffff) + (E_pre ? (1 << 23) : 0);

	if (!M) return 0;

	FLOAT R = E >= 7 ? M << (E-7) : M >> (7-E);
	return (_a >> 31) ? -R : R;
}

FLOAT Fabs(FLOAT a) {
	return a >= 0 ? a : (~a+1) & (~0u >> 1);
}

/* Functions below are already implemented */

FLOAT sqrt(FLOAT x) {
	FLOAT dt, t = int2F(2);

	do {
		dt = F_div_int((F_div_F(x, t) - t), 2);
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

FLOAT pow(FLOAT x, FLOAT y) {
	/* we only compute x^0.333 */
	FLOAT t2, dt, t = int2F(2);

	do {
		t2 = F_mul_F(t, t);
		dt = (F_div_F(x, t2) - t) / 3;
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

