// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_FX_SAFE_TYPES_H_
#define CORE_FXCRT_FX_SAFE_TYPES_H_

#include <stdlib.h>  // For size_t.

#include "core/fxcrt/fx_stream.h"  // for FX_FILESIZE.
#include "core/fxcrt/fx_system.h"
#include "third_party/base/numerics/safe_math.h"

typedef pdfium::base::CheckedNumeric<uint32_t> FX_SAFE_UINT32;
typedef pdfium::base::CheckedNumeric<int32_t> FX_SAFE_INT32;
typedef pdfium::base::CheckedNumeric<size_t> FX_SAFE_SIZE_T;
typedef pdfium::base::CheckedNumeric<FX_FILESIZE> FX_SAFE_FILESIZE;
typedef pdfium::base::CheckedNumeric<FX_STRSIZE> FX_SAFE_STRSIZE;

#endif  // CORE_FXCRT_FX_SAFE_TYPES_H_
