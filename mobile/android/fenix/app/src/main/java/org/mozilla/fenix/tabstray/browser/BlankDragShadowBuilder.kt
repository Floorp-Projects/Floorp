/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.graphics.Canvas
import android.graphics.Point
import android.view.View

class BlankDragShadowBuilder : View.DragShadowBuilder() {
    override fun onProvideShadowMetrics(outShadowSize: Point?, outShadowTouchPoint: Point?) {
        outShadowSize?.x = 1
        outShadowSize?.y = 1
        outShadowTouchPoint?.x = 0
        outShadowTouchPoint?.y = 0
    }

    override fun onDrawShadow(canvas: Canvas?) {
        // Do nothing
    }
}
