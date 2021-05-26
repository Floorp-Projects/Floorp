// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PLUGINS_GIMP_FILE_JXL_SAVE_H_
#define PLUGINS_GIMP_FILE_JXL_SAVE_H_

#include <libgimp/gimp.h>

#include "lib/jxl/base/status.h"

namespace jxl {

Status SaveJpegXlImage(gint32 image_id, gint32 drawable_id,
                       gint32 orig_image_id, const gchar* filename);

}

#endif  // PLUGINS_GIMP_FILE_JXL_SAVE_H_
