/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/****************************************************************************
*
*   Module Title :     type_aliases.h
*
*   Description  :     Standard type aliases
*
****************************************************************************/
#ifndef __INC_TYPE_ALIASES_H
#define __INC_TYPE_ALIASES_H

/****************************************************************************
* Macros
****************************************************************************/
#define EXPORT
#define IMPORT          extern      /* Used to declare imported data & routines */
#define PRIVATE         static      /* Used to declare & define module-local data */
#define LOCAL           static      /* Used to define all persistent routine-local data */
#define STD_IN_PATH     0           /* Standard input path */
#define STD_OUT_PATH    1           /* Standard output path */
#define STD_ERR_PATH    2           /* Standard error path */
#define STD_IN_FILE     stdin       /* Standard input file pointer */
#define STD_OUT_FILE    stdout      /* Standard output file pointer */
#define STD_ERR_FILE    stderr      /* Standard error file pointer */
#define max_int         0x7FFFFFFF

#define __export
#define _export

#define CCONV

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

/****************************************************************************
* Typedefs
****************************************************************************/
#ifndef TYPE_INT8
#define TYPE_INT8
typedef signed char     INT8;
#endif

#ifndef TYPE_INT16
//#define TYPE_INT16
typedef signed short    INT16;
#endif

#ifndef TYPE_INT32
//#define TYPE_INT32
typedef signed int      INT32;
#endif

#ifndef TYPE_UINT8
//#define TYPE_UINT8
typedef unsigned char   UINT8;
#endif

#ifndef TYPE_UINT32
//#define TYPE_UINT32
typedef unsigned int    UINT32;
#endif

#ifndef TYPE_UINT16
//#define TYPE_UINT16
typedef unsigned short  UINT16;
#endif

#ifndef TYPE_BOOL
//#define TYPE_BOOL
typedef int             BOOL;
#endif

typedef unsigned char   BOOLEAN;

#ifdef _MSC_VER
typedef __int64 INT64;
#else

#ifndef TYPE_INT64
#ifdef _TMS320C6X
//for now we only have 40bits
typedef long INT64;
#else
typedef long long INT64;
#endif
#endif

#endif

/* Floating point */
typedef  double         FLOAT64;
typedef  float          FLOAT32;

#endif
