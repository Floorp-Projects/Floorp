/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef I420_COLOR_CONVERTER_H
#define I420_COLOR_CONVERTER_H

#include <II420ColorConverter.h>

// This is a wrapper around the I420 color converter functions in
// II420ColorConverter, which is loaded from a shared library.
class I420ColorConverter: public II420ColorConverter {
public:
    I420ColorConverter();
    ~I420ColorConverter();

    // Returns true if the converter functions are successfully loaded.
    bool isLoaded();
private:
    void* mHandle;
};

#endif /* I420_COLOR_CONVERTER_H */
