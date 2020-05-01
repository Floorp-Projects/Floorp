/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import androidx.recyclerview.widget.ItemTouchHelper
import mozilla.components.browser.tabstray.TabTouchCallback
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable

class TabsTouchHelper(observable: Observable<TabsTray.Observer>) :
    ItemTouchHelper(object : TabTouchCallback(observable) {
        override fun alphaForItemSwipe(dX: Float, distanceToAlphaMin: Int): Float {
            return 1f - 2f * Math.abs(dX) / distanceToAlphaMin
        }
    })
