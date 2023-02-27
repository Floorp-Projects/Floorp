// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_ERROR_H_
#define LIB_JPEGLI_ERROR_H_

/* clang-format off */
#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
#include <stdarg.h>
/* clang-format on */

namespace jpegli {

bool FormatString(char* buffer, const char* format, ...);

}  // namespace jpegli

#define JPEGLI_ERROR(format, ...)                                       \
  jpegli::FormatString(cinfo->err->msg_parm.s, ("%s:%d: " format "\n"), \
                       __FILE__, __LINE__, ##__VA_ARGS__),              \
      (*cinfo->err->error_exit)(reinterpret_cast<j_common_ptr>(cinfo))

#endif  // LIB_JPEGLI_ERROR_H_
