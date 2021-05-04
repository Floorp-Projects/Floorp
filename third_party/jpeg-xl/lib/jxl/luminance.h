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

#ifndef LIB_JXL_LUMINANCE_H_
#define LIB_JXL_LUMINANCE_H_

namespace jxl {

// Chooses a default intensity target based on the transfer function of the
// image, if known. For SDR images or images not known to be HDR, returns
// kDefaultIntensityTarget, for images known to have PQ or HLG transfer function
// returns a higher value. If the image metadata already has a non-zero
// intensity target, does nothing.
class CodecInOut;
void SetIntensityTarget(CodecInOut* io);

}  // namespace jxl

#endif  // LIB_JXL_LUMINANCE_H_
