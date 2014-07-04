/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
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

#include "Framebuffer.h"

#include "android/log.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include "nsSize.h"
#include "mozilla/FileUtils.h"

#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)

using namespace std;

namespace mozilla {
namespace Framebuffer {

static size_t sMappedSize;
static struct fb_var_screeninfo sVi;
static gfxIntSize *sScreenSize = nullptr;

bool
GetSize(nsIntSize *aScreenSize) {
    // If the framebuffer has been opened, we should always have the size.
    if (sScreenSize) {
        *aScreenSize = *sScreenSize;
        return true;
    }

    ScopedClose fd(open("/dev/graphics/fb0", O_RDWR));
    if (0 > fd.get()) {
        LOG("Error opening framebuffer device");
        return false;
    }

    if (0 > ioctl(fd.get(), FBIOGET_VSCREENINFO, &sVi)) {
        LOG("Error getting variable screeninfo");
        return false;
    }

    sScreenSize = new gfxIntSize(sVi.xres, sVi.yres);
    *aScreenSize = *sScreenSize;
    return true;
}

} // namespace Framebuffer
} // namespace mozilla
