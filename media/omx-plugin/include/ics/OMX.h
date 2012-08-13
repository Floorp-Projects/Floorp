/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_OMX_H_
#define ANDROID_OMX_H_

#include <media/IOMX.h>
#include <utils/threads.h>
#include <utils/KeyedVector.h>

#if !defined(STAGEFRIGHT_EXPORT)
#define STAGEFRIGHT_EXPORT
#endif

namespace android {

struct OMXMaster;
class OMXNodeInstance;
class STAGEFRIGHT_EXPORT OMX {
public:
  OMX();
  virtual ~OMX();

private:
  char reserved[96];
};
}  // namespace android

#endif  // ANDROID_OMX_H_
