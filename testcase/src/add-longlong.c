#include "trap.h"

long long res;
int i, j, ans_idx = 0;
int loop = 0;
long long test_data[] = {0, 1, 2, 0x7fffffffffffffffLL, 0x8000000000000000LL, 0x8000000000000001LL, 0xfffffffffffffffeLL, 0xffffffffffffffffLL};
long long ans[] = {0LL, 0x1LL, 0x2LL, 0x7fffffffffffffffLL, 0x8000000000000000LL, 0x8000000000000001LL, 0xfffffffffffffffeLL, 0xffffffffffffffffLL, 0x1LL, 0x2LL, 0x3LL, 0x8000000000000000LL, 0x8000000000000001LL, 0x8000000000000002LL, 0xffffffffffffffffLL, 0LL, 0x2LL, 0x3LL, 0x4LL, 0x8000000000000001LL, 0x8000000000000002LL, 0x8000000000000003LL, 0LL, 0x1LL, 0x7fffffffffffffffLL, 0x8000000000000000LL, 0x8000000000000001LL, 0xfffffffffffffffeLL, 0xffffffffffffffffLL, 0LL, 0x7ffffffffffffffdLL, 0x7ffffffffffffffeLL, 0x8000000000000000LL, 0x8000000000000001LL, 0x8000000000000002LL, 0xffffffffffffffffLL, 0LL, 0x1LL, 0x7ffffffffffffffeLL, 0x7fffffffffffffffLL, 0x8000000000000001LL, 0x8000000000000002LL, 0x8000000000000003LL, 0LL, 0x1LL, 0x2LL, 0x7fffffffffffffffLL, 0x8000000000000000LL, 0xfffffffffffffffeLL, 0xffffffffffffffffLL, 0LL, 0x7ffffffffffffffdLL, 0x7ffffffffffffffeLL, 0x7fffffffffffffffLL, 0xfffffffffffffffcLL, 0xfffffffffffffffdLL, 0xffffffffffffffffLL, 0LL, 0x1LL, 0x7ffffffffffffffeLL, 0x7fffffffffffffffLL, 0x8000000000000000LL, 0xfffffffffffffffdLL, 0xfffffffffffffffeLL};
#define NR_DATA (sizeof(test_data) / sizeof(test_data[0]))

long long add(long long a, long long b) {
	if (i==3) set_bp();
	long long c = a + b;
	return c;
}

int main() {
	//set_bp();
	for(i = 0; i < NR_DATA; i ++) {
		for(j = 0; j < NR_DATA; j ++) {
			if (i==3) set_bp();
			res = add(test_data[i], test_data[j]);
			if (i==3) set_bp();
			nemu_assert(res == ans[ans_idx ++]);
			loop ++;
			if (i==3) set_bp();
		}
	}

	nemu_assert(loop == NR_DATA * NR_DATA);

	return 0;
}
