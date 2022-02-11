/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import org.mozilla.focus.R.color
import org.mozilla.focus.ui.theme.FocusTheme
import kotlin.math.roundToInt

private const val INDICATOR_BASE_TO_HEIGHT_RATIO = 1.5f

/**
 * A [Shape] describing a popup with an indicator triangle shown at top.
 *
 * @param indicatorArrowStartOffset Distance between the popup start and the indicator arrow start
 * @param indicatorArrowHeight Height of the indicator triangle. This influences the base length.
 * @param cornerRadius The radius of the popup's corners.
 * If [indicatorArrowStartOffset] is `0` then the top-start corner will not be rounded.
 */
class CFRPopupShape(
    private val indicatorArrowStartOffset: Dp,
    private val indicatorArrowHeight: Dp,
    private val cornerRadius: Dp
) : Shape {
    override fun createOutline(
        size: Size,
        layoutDirection: LayoutDirection,
        density: Density
    ): Outline {
        val indicatorArrowStartOffsetPx = indicatorArrowStartOffset.value * density.density
        val indicatorArrowHeightPx = indicatorArrowHeight.value * density.density
        val indicatorArrowBasePx =
            getIndicatorBaseWidthForHeight((indicatorArrowHeight.value * density.density).roundToInt())
        val cornerRadiusPx = cornerRadius.value * density.density

        return Outline.Generic(
            path = Path().apply {
                reset()

                lineTo(0f, size.height - cornerRadiusPx)
                quadraticBezierTo(
                    0f, size.height,
                    cornerRadiusPx, size.height
                )

                lineTo(size.width - cornerRadiusPx, size.height)
                quadraticBezierTo(
                    size.width, size.height,
                    size.width, size.height - cornerRadiusPx
                )

                val indicatorCornerRadius = cornerRadiusPx.coerceAtMost(indicatorArrowStartOffsetPx)
                if (layoutDirection == LayoutDirection.Ltr) {
                    lineTo(size.width, cornerRadiusPx + indicatorArrowHeightPx)
                    quadraticBezierTo(
                        size.width, indicatorArrowHeightPx,
                        size.width - cornerRadiusPx, indicatorArrowHeightPx
                    )

                    lineTo(indicatorArrowStartOffsetPx + indicatorArrowBasePx, indicatorArrowHeightPx)
                    lineTo(indicatorArrowStartOffsetPx + indicatorArrowBasePx / 2, 0f)
                    lineTo(indicatorArrowStartOffsetPx, indicatorArrowHeightPx)

                    lineTo(indicatorCornerRadius, indicatorArrowHeightPx)
                    quadraticBezierTo(
                        0f, indicatorArrowHeightPx, 0f,
                        indicatorArrowHeightPx + indicatorCornerRadius
                    )
                } else {
                    lineTo(size.width, indicatorCornerRadius + indicatorArrowHeightPx)
                    quadraticBezierTo(
                        size.width, indicatorArrowHeightPx,
                        size.width - indicatorCornerRadius, indicatorArrowHeightPx
                    )

                    val indicatorEnd = size.width - indicatorArrowStartOffsetPx
                    lineTo(indicatorEnd, indicatorArrowHeightPx)
                    lineTo(indicatorEnd - indicatorArrowBasePx / 2, 0f)
                    lineTo(indicatorEnd - indicatorArrowBasePx, indicatorArrowHeightPx)

                    lineTo(cornerRadiusPx, indicatorArrowHeightPx)
                    quadraticBezierTo(
                        0f, indicatorArrowHeightPx,
                        0f, indicatorArrowHeightPx + cornerRadiusPx
                    )
                }

                close()
            }
        )
    }

    companion object {
        /**
         * This [Shape]'s arrow indicator will have an automatic width depending on the set height.
         * This method allows knowing what the base width will be before instantiating the class.
         */
        fun getIndicatorBaseWidthForHeight(height: Int): Int {
            return (height * INDICATOR_BASE_TO_HEIGHT_RATIO).roundToInt()
        }
    }
}

@Preview
@Composable
private fun CFRPopupShapeLTRPreview() {
    FocusTheme {
        Box(
            modifier = Modifier
                .height(100.dp)
                .width(200.dp)
                .background(
                    shape = CFRPopupShape(10.dp, 10.dp, 10.dp),
                    brush = Brush.linearGradient(
                        colors = listOf(
                            colorResource(color.cfr_pop_up_shape_end_color),
                            colorResource(color.cfr_pop_up_shape_start_color)
                        ),
                        end = Offset(0f, Float.POSITIVE_INFINITY),
                        start = Offset(Float.POSITIVE_INFINITY, 0f)
                    )
                ),
            contentAlignment = Alignment.Center
        ) {
            Text(text = "This is just a test")
        }
    }
}

@Preview
@Composable
private fun CFRPopupShapeRTLPreview() {
    FocusTheme {
        CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
            Box(
                modifier = Modifier
                    .height(100.dp)
                    .width(200.dp)
                    .background(
                        shape = CFRPopupShape(10.dp, 10.dp, 10.dp),
                        brush = Brush.linearGradient(
                            colors = listOf(
                                colorResource(color.cfr_pop_up_shape_end_color),
                                colorResource(color.cfr_pop_up_shape_start_color)
                            ),
                            end = Offset(0f, Float.POSITIVE_INFINITY),
                            start = Offset(Float.POSITIVE_INFINITY, 0f)
                        )
                    ),
                contentAlignment = Alignment.Center
            ) {
                Text(text = "This is just a test")
            }
        }
    }
}
