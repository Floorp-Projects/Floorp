/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#ifndef BYTESWAP_H_
#define BYTESWAP_H_

#include <sys/endian.h>

#ifdef __OpenBSD__
#define bswap_16(x)	swap16(x)
#else
#define bswap_16(x)	bswap16(x)
#endif

#endif
