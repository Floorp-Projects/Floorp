/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2013 Mozilla Foundation
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

#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <cutils/log.h>

#include <gui/IDisplayEventConnection.h>
#include <gui/GraphicBufferAlloc.h>

#include "FakeSurfaceComposer.h"

namespace android {

/* static */
void FakeSurfaceComposer::instantiate() {
    defaultServiceManager()->addService(
            String16("SurfaceFlinger"), new FakeSurfaceComposer());
}

FakeSurfaceComposer::FakeSurfaceComposer()
    : BnSurfaceComposer()
{
}

FakeSurfaceComposer::~FakeSurfaceComposer()
{
}

sp<ISurfaceComposerClient> FakeSurfaceComposer::createConnection()
{
    return nullptr;
}

sp<IGraphicBufferAlloc> FakeSurfaceComposer::createGraphicBufferAlloc()
{
    sp<GraphicBufferAlloc> gba(new GraphicBufferAlloc());
    return gba;
}

sp<IBinder> FakeSurfaceComposer::createDisplay(const String8& displayName,
        bool secure)
{
    return nullptr;
}

sp<IBinder> FakeSurfaceComposer::getBuiltInDisplay(int32_t id) {
    return nullptr;
}

void FakeSurfaceComposer::setTransactionState(
        const Vector<ComposerState>& state,
        const Vector<DisplayState>& displays,
        uint32_t flags)
{
}

void FakeSurfaceComposer::bootFinished()
{
}

bool FakeSurfaceComposer::authenticateSurfaceTexture(
        const sp<IGraphicBufferProducer>& bufferProducer) const {
    return false;
}

sp<IDisplayEventConnection> FakeSurfaceComposer::createDisplayEventConnection() {
    return nullptr;
}

status_t FakeSurfaceComposer::captureScreen(const sp<IBinder>& display,
        const sp<IGraphicBufferProducer>& producer,
        uint32_t reqWidth, uint32_t reqHeight,
        uint32_t minLayerZ, uint32_t maxLayerZ,
        bool isCpuConsumer) {
    return INVALID_OPERATION;
}

void FakeSurfaceComposer::blank(const sp<IBinder>& display) {
}

void FakeSurfaceComposer::unblank(const sp<IBinder>& display) {
}

status_t FakeSurfaceComposer::getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info) {
    return INVALID_OPERATION;
}

}; // namespace android
