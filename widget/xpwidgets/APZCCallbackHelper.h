/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_APZCCallbackHelper_h__
#define __mozilla_widget_APZCCallbackHelper_h__

#include "FrameMetrics.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindowUtils.h"

namespace mozilla {
namespace widget {

/* This class contains some helper methods that facilitate implementing the
   GeckoContentController callback interface required by the AsyncPanZoomController.
   Since different platforms need to implement this interface in similar-but-
   not-quite-the-same ways, this utility class provides some helpful methods
   to hold code that can be shared across the different platform implementations.
 */
class APZCCallbackHelper
{
    typedef mozilla::layers::FrameMetrics FrameMetrics;

public:
    /* Checks to see if the pres shell that the given FrameMetrics object refers
       to is still the valid pres shell for the DOMWindowUtils. This can help
       guard against apply stale updates (updates meant for a pres shell that has
       since been torn down and replaced). */
    static bool HasValidPresShellId(nsIDOMWindowUtils* aUtils,
                                    const FrameMetrics& aMetrics);

    /* Applies the scroll and zoom parameters from the given FrameMetrics object to
       the root frame corresponding to the given DOMWindowUtils. */
    static void UpdateRootFrame(nsIDOMWindowUtils* aUtils,
                                const FrameMetrics& aMetrics);

    /* Applies the scroll parameters from the given FrameMetrics object to the subframe
       corresponding to the given content object. */
    static void UpdateSubFrame(nsIContent* aContent,
                               const FrameMetrics& aMetrics);

    /* Get the DOMWindowUtils for the window corresponding to the given document. */
    static already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils(const nsIDocument* aDoc);

    /* Get the DOMWindowUtils for the window corresponding to the givent content
       element. This might be an iframe inside the tab, for instance. */
    static already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils(const nsIContent* aContent);

    /* Get the presShellId and view ID for the given content element, if they can be
       found. Returns false if the values could not be found, true if they could. */
    static bool GetScrollIdentifiers(const nsIContent* aContent,
                                     uint32_t* aPresShellIdOut,
                                     FrameMetrics::ViewID* aViewIdOut);
};

}
}

#endif /*__mozilla_widget_APZCCallbackHelper_h__ */
