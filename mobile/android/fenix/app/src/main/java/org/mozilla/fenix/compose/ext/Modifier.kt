/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.ext

import android.os.SystemClock
import androidx.compose.foundation.clickable
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
import androidx.compose.ui.unit.Dp

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
