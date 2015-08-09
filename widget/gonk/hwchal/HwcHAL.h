/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#ifndef mozilla_HwcHAL
#define mozilla_HwcHAL

#include "HwcHALBase.h"

namespace mozilla {

class HwcHAL final : public HwcHALBase {
public:
    explicit HwcHAL();

    virtual ~HwcHAL();

    virtual bool HasHwc() const override { return static_cast<bool>(mHwc); }

    virtual void SetEGLInfo(hwc_display_t aDpy,
                            hwc_surface_t aSur) override { }

    virtual bool Query(QueryType aType) override;

    virtual int Set(HwcList *aList,
                    uint32_t aDisp) override;

    virtual int ResetHwc() override;

    virtual int Prepare(HwcList *aList,
                        uint32_t aDisp,
                        hwc_rect_t aDispRect,
                        buffer_handle_t aHandle,
                        int aFenceFd) override;

    virtual bool SupportTransparency() const override;

    virtual uint32_t GetGeometryChangedFlag(bool aGeometryChanged) const override;

    virtual void SetCrop(HwcLayer &aLayer,
                         const hwc_rect_t &aSrcCrop) const override;

    virtual bool EnableVsync(bool aEnable) override;

    virtual bool RegisterHwcEventCallback(const HwcHALProcs_t &aProcs) override;

private:
    uint32_t GetAPIVersion() const;

private:
    HwcDevice  *mHwc = nullptr;
};

} // namespace mozilla

#endif // mozilla_HwcHAL
