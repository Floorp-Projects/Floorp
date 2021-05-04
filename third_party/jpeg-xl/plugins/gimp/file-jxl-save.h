// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLUGINS_GIMP_FILE_JXL_SAVE_H_
#define PLUGINS_GIMP_FILE_JXL_SAVE_H_

#include <libgimp/gimp.h>

#include "lib/jxl/base/status.h"

namespace jxl {

Status SaveJpegXlImage(gint32 image_id, gint32 drawable_id,
                       gint32 orig_image_id, const gchar* filename);

}

#endif  // PLUGINS_GIMP_FILE_JXL_SAVE_H_
