#ifndef __ROMFS_NEO_H__
#define __ROMFS_NEO_H__

#include <stdint.h>

#define ROMFS_OPENFAIL (-1)

void register_romfs_neo(const char *mountpoint, const uint8_t * romfs);
const uint8_t *romfs_neo_get_file_by_hash(const uint8_t * romfs, uint32_t h,
				      uint32_t * len);

#endif /* __ROMFS_NEO_H__ */
