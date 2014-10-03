/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentProcessController.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "base/message_loop.h"

namespace mozilla {
namespace widget {

void
ParentProcessController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
    MOZ_ASSERT(NS_IsMainThread());

    if (aFrameMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aFrameMetrics.GetScrollId());
    if (content) {
        FrameMetrics metrics = aFrameMetrics;
        mozilla::layers::APZCCallbackHelper::UpdateSubFrame(content, metrics);
    }
}

void
ParentProcessController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                                 const uint32_t& aScrollGeneration)
{
    mozilla::layers::APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
}

void
ParentProcessController::PostDelayedTask(Task* aTask, int aDelayMs)
{
    MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

}
}
