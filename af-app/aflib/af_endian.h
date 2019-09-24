#pragma once

#include <stdint.h>
#include <endian.h>

/* on qsdk, we can't assume that htole16 and friends actually exist */
#include <byteswap.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define af_htole16(_x) (_x)
#define af_htole32(_x) (_x)
#define af_htole64(_x) (_x)
#define af_letoh16(_x) (_x)
#define af_letoh32(_x) (_x)
#define af_letoh64(_x) (_x)
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#define af_htole16(_x) bswap_16(_x)
#define af_htole32(_x) bswap_32(_x)
#define af_htole64(_x) bswap_64(_x)
#define af_letoh16(_x) bswap_16(_x)
#define af_letoh32(_x) bswap_32(_x)
#define af_letoh64(_x) bswap_64(_x)
#endif
