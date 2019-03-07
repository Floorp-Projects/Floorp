/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.annotation.SuppressLint
import android.content.Context
import androidx.core.view.NestedScrollingChild
import androidx.core.view.NestedScrollingChildHelper
import androidx.core.view.ViewCompat
import android.view.MotionEvent
import androidx.annotation.VisibleForTesting
import org.mozilla.geckoview.GeckoView

/**
 * geckoView that supports nested scrolls (for using in a CoordinatorLayout).
 *
 * This code is a simplified version of the NestedScrollView implementation
 * which can be found in the support library:
 * [android.support.v4.widget.NestedScrollView]
 *
 * Based on:
 * https://github.com/takahirom/webview-in-coordinatorlayout
 */

open class NestedGeckoView(context: Context) : GeckoView(context), NestedScrollingChild {

    @VisibleForTesting
    internal var lastY: Int = 0

    @VisibleForTesting
    internal val scrollOffset = IntArray(2)

    private val scrollConsumed = IntArray(2)

    @VisibleForTesting
    internal var nestedOffsetY: Int = 0

    @VisibleForTesting
    internal var childHelper: NestedScrollingChildHelper = NestedScrollingChildHelper(this)

    init {
        isNestedScrollingEnabled = true
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(ev: MotionEvent): Boolean {
        val event = MotionEvent.obtain(ev)
        val action = ev.actionMasked
        val eventY = event.y.toInt()

        if (action == MotionEvent.ACTION_DOWN) {
            nestedOffsetY = 0
        }

        when (action) {
            MotionEvent.ACTION_MOVE -> {
                val allowScroll = !shouldPinOnScreen()
                var deltaY = lastY - eventY

                if (allowScroll && dispatchNestedPreScroll(0, deltaY, scrollConsumed, scrollOffset)) {
                    deltaY -= scrollConsumed[1]
                    event.offsetLocation(0f, (-scrollOffset[1]).toFloat())
                    nestedOffsetY += scrollOffset[1]
                }

                lastY = eventY - scrollOffset[1]

                if (allowScroll && dispatchNestedScroll(0, scrollOffset[1], 0, deltaY, scrollOffset)) {
                    lastY -= scrollOffset[1]
                    event.offsetLocation(0f, scrollOffset[1].toFloat())
                    nestedOffsetY += scrollOffset[1]
                }
            }

            MotionEvent.ACTION_DOWN -> {
                lastY = eventY
                startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)
            }
            // We don't care about other touch events
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> stopNestedScroll()
        }

        // Execute event handler from parent class in all cases
        val eventHandled = super.onTouchEvent(event)

        // Recycle previously obtained event
        event.recycle()

        return eventHandled
    }

    override fun setNestedScrollingEnabled(enabled: Boolean) {
        childHelper.isNestedScrollingEnabled = enabled
    }

    override fun isNestedScrollingEnabled(): Boolean {
        return childHelper.isNestedScrollingEnabled
    }

    override fun startNestedScroll(axes: Int): Boolean {
        return childHelper.startNestedScroll(axes)
    }

    override fun stopNestedScroll() {
        childHelper.stopNestedScroll()
    }

    override fun hasNestedScrollingParent(): Boolean {
        return childHelper.hasNestedScrollingParent()
    }

    override fun dispatchNestedScroll(
        dxConsumed: Int,
        dyConsumed: Int,
        dxUnconsumed: Int,
        dyUnconsumed: Int,
        offsetInWindow: IntArray?
    ): Boolean {
        return childHelper.dispatchNestedScroll(dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed, offsetInWindow)
    }

    override fun dispatchNestedPreScroll(dx: Int, dy: Int, consumed: IntArray?, offsetInWindow: IntArray?): Boolean {
        return childHelper.dispatchNestedPreScroll(dx, dy, consumed, offsetInWindow)
    }

    override fun dispatchNestedFling(velocityX: Float, velocityY: Float, consumed: Boolean): Boolean {
        return childHelper.dispatchNestedFling(velocityX, velocityY, consumed)
    }

    override fun dispatchNestedPreFling(velocityX: Float, velocityY: Float): Boolean {
        return childHelper.dispatchNestedPreFling(velocityX, velocityY)
    }
}
