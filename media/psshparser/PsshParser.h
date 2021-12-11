/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ClearKeyCencParser_h__
#define __ClearKeyCencParser_h__

#include <stdint.h>
#include <vector>

#define CENC_KEY_LEN ((size_t)16)

bool ParseCENCInitData(const uint8_t* aInitData, uint32_t aInitDataSize,
                       std::vector<std::vector<uint8_t>>& aOutKeyIds);

#endif
