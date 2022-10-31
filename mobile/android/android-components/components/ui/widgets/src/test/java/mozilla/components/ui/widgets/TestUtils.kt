/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets

import android.view.MotionEvent

object TestUtils {
    fun getMotionEvent(
        action: Int,
        x: Float = 0f,
        y: Float = 0f,
        eventTime: Long = System.currentTimeMillis(),
        previousEvent: MotionEvent? = null,
    ): MotionEvent {
        val downTime = previousEvent?.downTime ?: System.currentTimeMillis()

        var pointerCount = previousEvent?.pointerCount ?: 0
        if (action == MotionEvent.ACTION_POINTER_DOWN) {
            pointerCount++
        } else if (action == MotionEvent.ACTION_DOWN) {
            pointerCount = 1
        }

        val properties = Array(
            pointerCount,
            TestUtils::getPointerProperties,
        )
        val pointerCoords =
            getPointerCoords(
                x,
                y,
                pointerCount,
                previousEvent,
            )

        return MotionEvent.obtain(
            downTime, eventTime,
            action, pointerCount, properties,
            pointerCoords, 0, 0, 1f, 1f, 0, 0, 0, 0,
        )
    }

    private fun getPointerCoords(
        x: Float,
        y: Float,
        pointerCount: Int,
        previousEvent: MotionEvent? = null,
    ): Array<MotionEvent.PointerCoords?> {
        val currentEventCoords = MotionEvent.PointerCoords().apply {
            this.x = x; this.y = y; pressure = 1f; size = 1f
        }

        return if (pointerCount > 1 && previousEvent != null) {
            arrayOf(
                MotionEvent.PointerCoords().apply {
                    this.x = previousEvent.x; this.y = previousEvent.y; pressure = 1f; size = 1f
                },
                currentEventCoords,
            )
        } else {
            arrayOf(currentEventCoords)
        }
    }

    private fun getPointerProperties(id: Int): MotionEvent.PointerProperties =
        MotionEvent.PointerProperties().apply {
            this.id = id; this.toolType = MotionEvent.TOOL_TYPE_FINGER
        }
}
