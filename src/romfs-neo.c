#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include "fio.h"
#include "filesystem.h"
#include "romfs-neo.h"
#include "osdebug.h"
#include "hash-djb2.h"

/********************************************************************
 * See more about romfs-neo, please checkout ../tool/mkromfs-neo.c
 ********************************************************************/
struct romfs_file_t
{
	uint32_t hash;
	uint8_t type;
	uint32_t param;
	uint8_t data;
}__attribute__((packed));

struct romfs_read_dir
{
	uint8_t length;
	uint8_t data;
}__attribute__((packed));

struct romfs_fds_t {
	const uint8_t *file;
	uint32_t cursor;
};

static struct romfs_fds_t romfs_fds[MAX_FDS];

static uint32_t get_unaligned(const uint8_t * d)
{
	return ((uint32_t) d[0]) | ((uint32_t) (d[1] << 8)) |
	    ((uint32_t) (d[2] << 16)) | ((uint32_t) (d[3] << 24));
}

static ssize_t romfs_read(void *opaque, void *buf, size_t count)
{
	struct romfs_fds_t *f = (struct romfs_fds_t *)opaque;
	const struct romfs_file_t *file_p =
		(struct romfs_file_t*)(f->file -
				      (sizeof(struct romfs_file_t) - 1));
	uint32_t size = file_p->param;

	if ((f->cursor + count) > size)
		count = size - f->cursor;

	memcpy(buf, f->file + f->cursor, count);
	f->cursor += count;

	return count;
}

static off_t romfs_seek(void *opaque, off_t offset, int whence)
{
	struct romfs_fds_t *f = (struct romfs_fds_t *)opaque;
	const uint8_t *size_p = f->file - 4;
	uint32_t size = get_unaligned(size_p);
	uint32_t origin;

	switch (whence) {
	case SEEK_SET:
		origin = 0;
		break;
	case SEEK_CUR:
		origin = f->cursor;
		break;
	case SEEK_END:
		origin = size;
		break;
	default:
		return -1;
	}

	offset = origin + offset;

	if (offset < 0)
		return -1;
	if (offset > size)
		offset = size;

	f->cursor = offset;

	return offset;
}

const uint8_t *romfs_neo_get_file_by_hash(const uint8_t * romfs, uint32_t h,
				      uint32_t * len)
{
	const uint8_t *meta;
	uint8_t type;
	uint32_t param, i;

	meta = romfs;

	while (((struct romfs_file_t*)meta)->hash &&
	       ((struct romfs_file_t*)meta)->type &&
	       ((struct romfs_file_t*)meta)->param) {

		type = ((struct romfs_file_t*)meta)->type;
		param = ((struct romfs_file_t*)meta)->param;

		/* Check if file(dir) exists */
		if (((struct romfs_file_t*)meta)->hash == h) {
			if (len)
				*len = ((struct romfs_file_t*)meta)->param;

			return &(((struct romfs_file_t*)meta)->data);
		}

		/* To next hash's address */
		if ((char)type == 'D') {
			meta += (sizeof(struct romfs_file_t) - 1);
			for(i = 0; i < param; i++) {
				meta += (((struct romfs_read_dir*)meta)->length)
					+ 1;
			}
		} else if ((char)type == 'F') {
			meta += ((struct romfs_file_t*)meta)->param +
				(sizeof(struct romfs_file_t) - 1);
		}
	}

	return NULL;
}

/**
 *  \brief
 *
 *  \param opaque
 *  \param path   The file path you wnat to find out
 *  \param flags  Currently no use
 *  \param mode   Currently no use
 *
 *  \returnjk:w
 */
static int romfs_open(void *opaque, const char *path, int flags, int mode)
{
	uint32_t h = hash_djb2((const uint8_t *)path, -1);
	const uint8_t *romfs = (const uint8_t *)opaque;
	const uint8_t *file_ptr;
	int fd = -1;

	file_ptr = romfs_neo_get_file_by_hash(romfs, h, NULL);

	if (file_ptr) {
		fd = fio_open(romfs_read, NULL, romfs_seek, NULL, NULL);
		if (fd > 0) {
			romfs_fds[fd].file = file_ptr;
			romfs_fds[fd].cursor = 0;
			/*fio_set_opaque(fd, (void*)romfs_fds[fd].file);*/
			fio_set_opaque(fd, romfs_fds + fd);
		}
	}
	return fd;
}

void register_romfs_neo(const char *mountpoint, const uint8_t * romfs)
{
//    DBGOUT("Registering romfs `%s' @ %p\r\n", mountpoint, romfs);
	register_fs(mountpoint, romfs_open, (void *)romfs);
}
