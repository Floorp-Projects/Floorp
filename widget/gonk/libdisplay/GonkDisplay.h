/* Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GONKDISPLAY_H
#define GONKDISPLAY_H

#include <system/window.h>

namespace mozilla {

typedef void * EGLDisplay;
typedef void * EGLSurface;

class GonkDisplay {
public:
    virtual ANativeWindow* GetNativeWindow() = 0;

    virtual void SetEnabled(bool enabled) = 0;

    virtual void* GetHWCDevice() = 0;

    virtual bool SwapBuffers(EGLDisplay dpy, EGLSurface sur) = 0;

    virtual ANativeWindowBuffer* DequeueBuffer() = 0;

    virtual bool QueueBuffer(ANativeWindowBuffer* buf) = 0;

    uint32_t xdpi;
    uint32_t surfaceformat;
};

__attribute__ ((weak))
GonkDisplay* GetGonkDisplay();

}
#endif /* GONKDISPLAY_H */
