#include "common.h"
#include <sys/ioctl.h>
#include <string.h>

typedef struct {
	char *name;
	uint32_t size;
	uint32_t disk_offset;
} file_info;

enum {SEEK_SET, SEEK_CUR, SEEK_END};

/* This is the information about all files in disk. */
static const file_info file_table[] __attribute__((used)) = {
	{"1.rpg", 188864, 1048576}, {"2.rpg", 188864, 1237440},
	{"3.rpg", 188864, 1426304}, {"4.rpg", 188864, 1615168},
	{"5.rpg", 188864, 1804032}, {"abc.mkf", 1022564, 1992896},
	{"ball.mkf", 134704, 3015460}, {"data.mkf", 66418, 3150164},
	{"desc.dat", 16027, 3216582}, {"fbp.mkf", 1128064, 3232609},
	{"fire.mkf", 834728, 4360673}, {"f.mkf", 186966, 5195401},
	{"gop.mkf", 11530322, 5382367}, {"map.mkf", 1496578, 16912689},
	{"mgo.mkf", 1577442, 18409267}, {"m.msg", 188232, 19986709},
	{"mus.mkf", 331284, 20174941}, {"pat.mkf", 8488, 20506225},
	{"rgm.mkf", 453202, 20514713}, {"rng.mkf", 4546074, 20967915},
	{"sss.mkf", 557004, 25513989}, {"voc.mkf", 1997044, 26070993},
	{"wor16.asc", 5374, 28068037}, {"wor16.fon", 82306, 28073411},
	{"word.dat", 5650, 28155717},
};

#define NR_FILES (sizeof(file_table) / sizeof(file_table[0]))

int fs_ioctl(int fd, uint32_t request, void *p) {
	assert(request == TCGETS);
	return (fd >= 0 && fd <= 2 ? 0 : -1);
}

void ide_read(uint8_t *, uint32_t, uint32_t);
void ide_write(uint8_t *, uint32_t, uint32_t);
/* TODO: implement a simplified file system here. */

typedef struct {
	bool opened;
	uint32_t offset;
} Fstate;

Fstate fstate[NR_FILES + 3];

static inline Fstate *state(int fd) {
	return &fstate[fd];
}

static inline int valid(int fd) {
	assert(fd >= 3 && fd < NR_FILES + 3);
	return state(fd)->opened;
}

static inline const file_info *query(int fd) {
	valid(fd);
	return &file_table[fd - 3];
}

static inline int overflow(int fd, int len) {
	assert(valid(fd));
	int of = state(fd)->offset + len - query(fd)->size;
	return (of > 0 ? of : 0);
}

int fs_open(const char *pathname, int flags) {
	int idx;
	int lqn, lpn = strlen(pathname);
	for (idx = 3; idx < NR_FILES + 3; ++idx) {
		lqn = strlen(query(idx)->name);
		if (lpn >= lqn && strcmp(query(idx)->name, pathname + lpn - lqn) == 0) {
			state(idx)->opened = true;
			state(idx)->offset = 0;
			break;
		}
	}

	assert(valid(idx));
	return idx;
}

int fs_read(int fd, void *buf, int len) {
	if (!valid(fd)) return -1;
	len -= overflow(fd, len);

	int real_off = query(fd)->disk_offset + state(fd)->offset;
	ide_read(buf, real_off, len);
	state(fd)->offset += len;
	return len;
}

int fs_write(int fd, void *buf, int len) {
	if (!valid(fd)) return -1;
	len -= overflow(fd, len);

	int real_off = query(fd)->disk_offset + state(fd)->offset;
	ide_write(buf, real_off, len);
	state(fd)->offset += len;
	return len;
}

int fs_lseek(int fd, int offset, int whence) {
	if (!valid(fd)) return -1;

	int res = state(fd)->offset;
	state(fd)->offset = 0;
	switch (whence) {
		case SEEK_SET:
			res = offset;
			break;
		case SEEK_CUR:
			res += offset;
			break;
		case SEEK_END:
			res = offset + query(fd)->size - 1;
			break;
		default:
			return -1;
	}

	res -= overflow(fd, res);
	return state(fd)->offset = res;
}

int fs_close(int fd) {
	if (!valid(fd)) return -1;

	state(fd)->opened = false;
	state(fd)->offset = 0;
	return 0;
}