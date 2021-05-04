# Copyright (c) the JPEG XL Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(PkgConfig)
pkg_check_modules(Gdk-Pixbuf IMPORTED_TARGET gdk-pixbuf-2.0>=2.38)

if (NOT Gdk-Pixbuf_FOUND)
  message(WARNING "GDK Pixbuf development libraries not found, \
                   the Gdk-Pixbuf plugin will not be built")
  return ()
endif ()

add_library(pixbufloader-jxl SHARED pixbufloader-jxl.c)
target_link_libraries(pixbufloader-jxl jxl-static jxl_threads-static PkgConfig::Gdk-Pixbuf)

pkg_get_variable(GDK_PIXBUF_MODULEDIR gdk-pixbuf-2.0 gdk_pixbuf_moduledir)
install(TARGETS pixbufloader-jxl LIBRARY DESTINATION "${GDK_PIXBUF_MODULEDIR}")

# Instead of the following, we might instead add the
# mime type image/jxl to
# /usr/share/thumbnailers/gdk-pixbuf-thumbnailer.thumbnailer
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/jxl.thumbnailer DESTINATION share/thumbnailers/)
