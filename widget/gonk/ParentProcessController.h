/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_DynamicToolbarController_h__
#define __mozilla_widget_DynamicToolbarController_h__

#include "mozilla/layers/GeckoContentController.h"

namespace mozilla {
namespace widget {

class ParentProcessController : public mozilla::layers::GeckoContentController
{
    typedef mozilla::layers::FrameMetrics FrameMetrics;

public:
    virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) MOZ_OVERRIDE;
    virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE;

    // No-ops
    virtual void HandleDoubleTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {}
    virtual void HandleSingleTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {}
    virtual void HandleLongTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {}
    virtual void HandleLongTapUp(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {}

    virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                         const CSSRect &aContentRect,
                                         const CSSSize &aScrollableSize) MOZ_OVERRIDE {}
};

}
}

#endif /*__mozilla_widget_DynamicToolbarController_h__ */
