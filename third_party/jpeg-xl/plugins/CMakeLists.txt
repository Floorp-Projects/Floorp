# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

if(NOT MSVC)
  add_subdirectory(gdk-pixbuf)
endif()

add_subdirectory(gimp)

add_subdirectory(mime)
