/* Written for GUSI by Matthias Neeracher <neeri@iis.ee.ethz.ch> */

#ifndef _INTTYPES_H_
#define _INTTYPES_H_

/*
 * Regrettably, this is needed for *int64_t
 */
#include <MacTypes.h>

typedef char				int8_t;
typedef short				int16_t;
typedef long				int32_t;
typedef SInt64				int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned long		uint32_t;
typedef UInt64				uint64_t;
typedef long				intptr_t;
typedef unsigned long		uintptr_t;

#endif /* _INTTYPES_H_ */
