// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PLUGINS_GIMP_FILE_JXL_LOAD_H_
#define PLUGINS_GIMP_FILE_JXL_LOAD_H_

#include <libgimp/gimp.h>

#include "lib/jxl/base/status.h"

namespace jxl {

Status LoadJpegXlImage(const gchar* filename, gint32* image_id);

}

#endif  // PLUGINS_GIMP_FILE_JXL_LOAD_H_
