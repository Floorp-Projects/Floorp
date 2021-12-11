/*
 * Copyright (c) 2011 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** OMX_ImageExt.h - OpenMax IL version 1.1.2
 * The OMX_ImageExt header file contains extensions to the
 * definitions used by both the application and the component to
 * access image items.
 */

#ifndef OMX_ImageExt_h
#define OMX_ImageExt_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header shall include all required header files to allow the
 * header to compile without errors.  The includes below are required
 * for this header file to compile successfully
 */
#include <OMX_Core.h>

/** Enum for standard image codingtype extensions */
typedef enum OMX_IMAGE_CODINGEXTTYPE {
    OMX_IMAGE_CodingExtUnused = OMX_IMAGE_CodingKhronosExtensions,
    OMX_IMAGE_CodingWEBP,         /**< WebP image format */
} OMX_IMAGE_CODINGEXTTYPE;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMX_ImageExt_h */
/* File EOF */
