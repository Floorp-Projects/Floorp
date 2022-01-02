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

add_library(lodepng STATIC EXCLUDE_FROM_ALL
  lodepng/lodepng.cpp
  lodepng/lodepng.h
)
# This library can be included into position independent binaries.
set_target_properties(lodepng PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_include_directories(lodepng
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/lodepng")
