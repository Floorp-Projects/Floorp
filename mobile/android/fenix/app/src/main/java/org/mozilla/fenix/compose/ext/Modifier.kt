/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.ext

import android.graphics.Rect
import android.os.SystemClock
import androidx.annotation.FloatRange
import androidx.compose.foundation.clickable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.LayoutCoordinates
import androidx.compose.ui.layout.boundsInWindow
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntSize
import kotlinx.coroutines.delay
import kotlin.math.max
import kotlin.math.min

/**
 * Add a dashed border around the current composable.
 *
 * @param color [Color] to apply to the dashed border.
 * @param cornerRadius The corner radius of the rounded rectangle.
 * @param dashHeight The thickness of a dash.
 * @param dashWidth The length of a dash.
 * @param dashGap The size of the gap between the dashes.
 */
fun Modifier.dashedBorder(
    color: Color,
    cornerRadius: Dp,
    dashHeight: Dp,
    dashWidth: Dp,
    dashGap: Dp = dashWidth,
) = this.then(
    drawBehind {
        val cornerRadiusPx = cornerRadius.toPx()
        val borderHeightPx = dashHeight.toPx()
        val dashWidthPx = dashWidth.toPx()
        val dashGapPx = dashGap.toPx()

        val dashedStroke = Stroke(
            width = borderHeightPx,
            pathEffect = PathEffect.dashPathEffect(
                floatArrayOf(dashWidthPx, dashGapPx),
                0f,
            ),
        )

        drawRoundRect(
            color = color,
            cornerRadius = CornerRadius(cornerRadiusPx),
            style = dashedStroke,
        )
    },
)

/**
 * Used when clickable needs to be debounced to prevent rapid successive clicks
 * from calling the onClick function.
 *
 * @param debounceInterval The length of time to wait between click events in milliseconds
 * @param onClick Callback for when item this modifier effects is clicked
 */
fun Modifier.debouncedClickable(
    debounceInterval: Long = 1000L,
    onClick: () -> Unit,
) = composed {
    var lastClickTime: Long by remember { mutableStateOf(0) }

    this.then(
        Modifier.clickable(
            onClick = {
                val currentSystemTime = SystemClock.elapsedRealtime()
                if (currentSystemTime - lastClickTime > debounceInterval) {
                    onClick()
                    lastClickTime = currentSystemTime
                }
            },
        ),
    )
}

/**
 * A conditional [Modifier.then] extension that allows chaining of conditional Modifiers.
 *
 * @param modifier The [Modifier] to return if the [predicate] is satisfied.
 * @param predicate The predicate used to determine which [Modifier] to return.
 *
 * @return the appropriate [Modifier] given the [predicate].
 */
fun Modifier.thenConditional(
    modifier: Modifier,
    predicate: () -> Boolean,
): Modifier =
    if (predicate()) {
        then(modifier)
    } else {
        this
    }

/**
 * The Composable this modifier is tied to may appear first and be fully constructed to then be pushed downwards
 * when other elements appear. This can lead to over-counting impressions with multiple such events
 * being possible without the user actually having time to see the UI or scrolling to it.
 */
private const val MINIMUM_TIME_TO_SETTLE_MS = 1000

/**
 * Add a callback for when this Composable is "shown" on the screen.
 * This checks whether the composable has at least [threshold] ratio of it's total area drawn inside
 * the screen bounds.
 * Does not account for other Views / Windows covering it.
 *
 * @param threshold The ratio of the total area to be within the screen bounds to trigger [onVisible].
 * @param settleTime The amount of time to wait before calling [onVisible].
 * @param onVisible Invoked when the UI is visible to the user.
 * @param screenBounds Optional override to specify the exact bounds to detect the on-screen visibility.
 */
fun Modifier.onShown(
    @FloatRange(from = 0.0, to = 1.0) threshold: Float,
    settleTime: Int = MINIMUM_TIME_TO_SETTLE_MS,
    onVisible: () -> Unit,
    screenBounds: Rect? = null,
): Modifier {
    val initialTime = System.currentTimeMillis()
    var lastVisibleCoordinates: LayoutCoordinates? = null

    return composed {
        var wasEventReported by remember { mutableStateOf(false) }
        val bounds = screenBounds ?: Rect().apply { LocalView.current.getWindowVisibleDisplayFrame(this) }

        // In the event this composable starts as visible but then gets pushed offscreen
        // before MINIMUM_TIME_TO_SETTLE_MS we will not report is as being visible.
        // In the LaunchedEffect we add support for when the composable starts as visible and then
        // it's position isn't changed after MINIMUM_TIME_TO_SETTLE_MS so it must be reported as visible.
        LaunchedEffect(initialTime) {
            delay(settleTime.toLong())
            if (!wasEventReported && lastVisibleCoordinates?.isVisible(bounds, threshold) == true) {
                wasEventReported = true
                onVisible()
            }
        }

        onGloballyPositioned { coordinates ->
            if (!wasEventReported && coordinates.isVisible(bounds, threshold)) {
                if (System.currentTimeMillis() - initialTime > settleTime) {
                    wasEventReported = true
                    onVisible()
                } else {
                    lastVisibleCoordinates = coordinates
                }
            }
        }
    }
}

/**
 * Return whether this has at least [threshold] ratio of it's total area drawn inside
 * the screen bounds.
 */
private fun LayoutCoordinates.isVisible(
    visibleRect: Rect,
    @FloatRange(from = 0.0, to = 1.0) threshold: Float,
): Boolean {
    if (!isAttached) return false

    val boundsInWindow = boundsInWindow()
    return Rect(
        boundsInWindow.left.toInt(),
        boundsInWindow.top.toInt(),
        boundsInWindow.right.toInt(),
        boundsInWindow.bottom.toInt(),
    ).getIntersectPercentage(size, visibleRect) >= threshold
}

/**
 * Returns the ratio of how much this intersects with [other].
 *
 * @param realSize [IntSize] containing the true height and width of the composable.
 * @param other Other [Rect] for which to check the intersection area.
 *
 * @return A `0..1` float range for how much this [Rect] intersects with other.
 */
@FloatRange(from = 0.0, to = 1.0)
private fun Rect.getIntersectPercentage(realSize: IntSize, other: Rect): Float {
    val composableArea = realSize.height * realSize.width
    val heightOverlap = max(0, min(bottom, other.bottom) - max(top, other.top))
    val widthOverlap = max(0, min(right, other.right) - max(left, other.left))
    val intersectionArea = heightOverlap * widthOverlap

    return (intersectionArea.toFloat() / composableArea)
}
