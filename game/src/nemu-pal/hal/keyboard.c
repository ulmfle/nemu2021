#include "hal.h"

#define NR_KEYS 18

//new
#define I8042_DATA_PORT 0x60

enum {KEY_STATE_EMPTY, KEY_STATE_WAIT_RELEASE, KEY_STATE_RELEASE, KEY_STATE_PRESS};

/* Only the following keys are used in NEMU-PAL. */
static const int keycode_array[] = {
	K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ESCAPE,
	K_RETURN, K_SPACE, K_PAGEUP, K_PAGEDOWN, K_r,
	K_a, K_d, K_e, K_w, K_q,
	K_s, K_f, K_p
};

static int key_state[NR_KEYS];

static inline bool
get_key_real(int scan_code, int *real_code_ret)
{
	*real_code_ret = scan_code & ~0x80;
	return !!(scan_code & 0x80);
}

void
keyboard_event() {
	/* TODO: Fetch the scancode and update the key states. */
	int idx;
	int scan_code = in_byte(I8042_DATA_PORT);
	bool keyup = get_key_real(scan_code, &scan_code);

	for (idx = 0; idx < NR_KEYS && keycode_array[idx] != scan_code; ++idx);

	key_state[idx] = keyup ? KEY_STATE_RELEASE : KEY_STATE_PRESS;
}

bool 
process_keys(void (*key_press_callback)(int), void (*key_release_callback)(int)) {
	cli();
	/* TODO: Traverse the key states. Find a key just pressed or released.
	 * If a pressed key is found, call `key_press_callback' with the keycode.
	 * If a released key is found, call `key_release_callback' with the keycode.
	 * If any such key is found, the function return true.
	 * If no such key is found, the function return false.
	 * Remember to enable interrupts before returning from the function.
	 */

	int idx;
	for (idx = 0; idx < NR_KEYS; ++idx) {
		if (key_state[idx] == KEY_STATE_PRESS) {
			key_press_callback(keycode_array[idx]);
			key_state[idx] = KEY_STATE_WAIT_RELEASE;
		} else if (key_state[idx] == KEY_STATE_RELEASE) {
			key_release_callback(keycode_array[idx]);
			key_state[idx] = KEY_STATE_EMPTY;
		} else {
			continue;
		}

		sti();
		return 1;
	}

	sti();
	return 0;
}
