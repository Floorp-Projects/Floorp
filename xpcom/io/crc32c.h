#ifndef crc32c_h
#define crc32c_h

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compute a CRC32c as defined in RFC3720.  This is a different polynomial than
// what is used in the crc for zlib, etc.  Typical usage to calculate a new CRC:
//
//    ComputeCrc32c(~0, buffer, bufferLength);
//
uint32_t
ComputeCrc32c(uint32_t aCrc, const void *aBuf, size_t aSize);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // crc32c_h
