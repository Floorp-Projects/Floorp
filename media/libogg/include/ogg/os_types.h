/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: Define a consistent set of types on each platform.

 ********************************************************************/
#ifndef _OS_TYPES_H
#define _OS_TYPES_H

#include <stddef.h>

/* We indirect mallocs through settable-at-runtime functions to accommodate
   memory reporting in the browser. */

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (ogg_malloc_function_type)(size_t);
typedef void* (ogg_calloc_function_type)(size_t, size_t);
typedef void* (ogg_realloc_function_type)(void*, size_t);
typedef void (ogg_free_function_type)(void*);

extern ogg_malloc_function_type *ogg_malloc_func;
extern ogg_calloc_function_type *ogg_calloc_func;
extern ogg_realloc_function_type *ogg_realloc_func;
extern ogg_free_function_type *ogg_free_func;

#ifdef __cplusplus
}
#endif

#define _ogg_malloc ogg_malloc_func
#define _ogg_calloc ogg_calloc_func
#define _ogg_realloc ogg_realloc_func
#define _ogg_free ogg_free_func

#if defined(_WIN32)

#  if defined(__CYGWIN__)
#    include <stdint.h>
     typedef int16_t ogg_int16_t;
     typedef uint16_t ogg_uint16_t;
     typedef int32_t ogg_int32_t;
     typedef uint32_t ogg_uint32_t;
     typedef int64_t ogg_int64_t;
     typedef uint64_t ogg_uint64_t;
#  elif defined(__MINGW32__)
#    include <sys/types.h>
     typedef short ogg_int16_t;
     typedef unsigned short ogg_uint16_t;
     typedef int ogg_int32_t;
     typedef unsigned int ogg_uint32_t;
     typedef long long ogg_int64_t;
     typedef unsigned long long ogg_uint64_t;
#  elif defined(__MWERKS__)
     typedef long long ogg_int64_t;
     typedef unsigned long long ogg_uint64_t;
     typedef int ogg_int32_t;
     typedef unsigned int ogg_uint32_t;
     typedef short ogg_int16_t;
     typedef unsigned short ogg_uint16_t;
#  else
#    if defined(_MSC_VER) && (_MSC_VER >= 1800) /* MSVC 2013 and newer */
#      include <stdint.h>
       typedef int16_t ogg_int16_t;
       typedef uint16_t ogg_uint16_t;
       typedef int32_t ogg_int32_t;
       typedef uint32_t ogg_uint32_t;
       typedef int64_t ogg_int64_t;
       typedef uint64_t ogg_uint64_t;
#    else
       /* MSVC/Borland */
       typedef __int64 ogg_int64_t;
       typedef __int32 ogg_int32_t;
       typedef unsigned __int32 ogg_uint32_t;
       typedef unsigned __int64 ogg_uint64_t;
       typedef __int16 ogg_int16_t;
       typedef unsigned __int16 ogg_uint16_t;
#    endif
#  endif

#elif (defined(__APPLE__) && defined(__MACH__)) /* MacOS X Framework build */

#  include <sys/types.h>
   typedef int16_t ogg_int16_t;
   typedef u_int16_t ogg_uint16_t;
   typedef int32_t ogg_int32_t;
   typedef u_int32_t ogg_uint32_t;
   typedef int64_t ogg_int64_t;
   typedef u_int64_t ogg_uint64_t;

#elif defined(__HAIKU__)

  /* Haiku */
#  include <sys/types.h>
   typedef short ogg_int16_t;
   typedef unsigned short ogg_uint16_t;
   typedef int ogg_int32_t;
   typedef unsigned int ogg_uint32_t;
   typedef long long ogg_int64_t;
   typedef unsigned long long ogg_uint64_t;

#elif defined(__BEOS__)

   /* Be */
#  include <inttypes.h>
   typedef int16_t ogg_int16_t;
   typedef uint16_t ogg_uint16_t;
   typedef int32_t ogg_int32_t;
   typedef uint32_t ogg_uint32_t;
   typedef int64_t ogg_int64_t;
   typedef uint64_t ogg_uint64_t;

#elif defined (__EMX__)

   /* OS/2 GCC */
   typedef short ogg_int16_t;
   typedef unsigned short ogg_uint16_t;
   typedef int ogg_int32_t;
   typedef unsigned int ogg_uint32_t;
   typedef long long ogg_int64_t;
   typedef unsigned long long ogg_uint64_t;


#elif defined (DJGPP)

   /* DJGPP */
   typedef short ogg_int16_t;
   typedef int ogg_int32_t;
   typedef unsigned int ogg_uint32_t;
   typedef long long ogg_int64_t;
   typedef unsigned long long ogg_uint64_t;

#elif defined(R5900)

   /* PS2 EE */
   typedef long ogg_int64_t;
   typedef unsigned long ogg_uint64_t;
   typedef int ogg_int32_t;
   typedef unsigned ogg_uint32_t;
   typedef short ogg_int16_t;

#elif defined(__SYMBIAN32__)

   /* Symbian GCC */
   typedef signed short ogg_int16_t;
   typedef unsigned short ogg_uint16_t;
   typedef signed int ogg_int32_t;
   typedef unsigned int ogg_uint32_t;
   typedef long long int ogg_int64_t;
   typedef unsigned long long int ogg_uint64_t;

#elif defined(__TMS320C6X__)

   /* TI C64x compiler */
   typedef signed short ogg_int16_t;
   typedef unsigned short ogg_uint16_t;
   typedef signed int ogg_int32_t;
   typedef unsigned int ogg_uint32_t;
   typedef long long int ogg_int64_t;
   typedef unsigned long long int ogg_uint64_t;

#else

#  include <ogg/config_types.h>

#endif

#endif  /* _OS_TYPES_H */
