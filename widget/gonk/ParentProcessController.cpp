/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentProcessController.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "APZCCallbackHelper.h"
#include "base/message_loop.h"

namespace mozilla {
namespace widget {

class RequestContentRepaintEvent : public nsRunnable
{
    typedef mozilla::layers::FrameMetrics FrameMetrics;

public:
    RequestContentRepaintEvent(const FrameMetrics& aFrameMetrics)
        : mFrameMetrics(aFrameMetrics)
    {
    }

    NS_IMETHOD Run() {
        MOZ_ASSERT(NS_IsMainThread());
        nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(mFrameMetrics.mScrollId);
        if (content) {
            APZCCallbackHelper::UpdateSubFrame(content, mFrameMetrics);
        }
        return NS_OK;
    }

protected:
    FrameMetrics mFrameMetrics;
};

void
ParentProcessController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
    if (aFrameMetrics.mScrollId == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    nsCOMPtr<nsIRunnable> r = new RequestContentRepaintEvent(aFrameMetrics);
    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(r);
    } else {
        r->Run();
    }
}

void
ParentProcessController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                                 const uint32_t& aScrollGeneration)
{
    APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
}

void
ParentProcessController::PostDelayedTask(Task* aTask, int aDelayMs)
{
    MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

}
}
