/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.cfr

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import mozilla.components.compose.cfr.CFRPopup.IndicatorDirection.DOWN
import mozilla.components.compose.cfr.CFRPopup.IndicatorDirection.UP
import kotlin.math.roundToInt

/**
 * How wide the base of the indicator should be in relation with the indicator's height.
 */
private const val INDICATOR_BASE_TO_HEIGHT_RATIO = 2f

/**
 * A [Shape] describing a popup with an indicator triangle shown above or below the popup.
 *
 * @param indicatorDirection The direction the indicator arrow is pointing to.
 * @param indicatorArrowStartOffset Distance between the popup start and the indicator arrow start
 * @param indicatorArrowHeight Height of the indicator triangle. This influences the base length.
 * @param cornerRadius The radius of the popup's corners.
 * If [indicatorArrowStartOffset] is `0` then the top-start corner will not be rounded.
 */
class CFRPopupShape(
    private val indicatorDirection: CFRPopup.IndicatorDirection,
    private val indicatorArrowStartOffset: Dp,
    private val indicatorArrowHeight: Dp,
    private val cornerRadius: Dp,
) : Shape {
    @Suppress("LongMethod")
    override fun createOutline(
        size: Size,
        layoutDirection: LayoutDirection,
        density: Density,
    ): Outline {
        val indicatorArrowStartOffsetPx = indicatorArrowStartOffset.value * density.density
        val indicatorArrowHeightPx = indicatorArrowHeight.value * density.density
        val indicatorArrowBasePx =
            getIndicatorBaseWidthForHeight((indicatorArrowHeight.value * density.density).roundToInt())
        val cornerRadiusPx = cornerRadius.value * density.density
        val indicatorCornerRadiusPx = cornerRadiusPx.coerceAtMost(indicatorArrowStartOffsetPx)

        // All outlines are drawn in a LTR space but with accounting for the LTR direction.
        return when (indicatorDirection) {
            CFRPopup.IndicatorDirection.UP -> {
                Outline.Generic(
                    path = Path().apply {
                        reset()

                        lineTo(0f, size.height - cornerRadiusPx)
                        quadraticBezierTo(
                            0f,
                            size.height,
                            cornerRadiusPx,
                            size.height,
                        )

                        lineTo(size.width - cornerRadiusPx, size.height)
                        quadraticBezierTo(
                            size.width,
                            size.height,
                            size.width,
                            size.height - cornerRadiusPx,
                        )

                        if (layoutDirection == LayoutDirection.Ltr) {
                            lineTo(size.width, cornerRadiusPx + indicatorArrowHeightPx)
                            quadraticBezierTo(
                                size.width,
                                indicatorArrowHeightPx,
                                size.width - cornerRadiusPx,
                                indicatorArrowHeightPx,
                            )

                            lineTo(indicatorArrowStartOffsetPx + indicatorArrowBasePx, indicatorArrowHeightPx)
                            lineTo(indicatorArrowStartOffsetPx + indicatorArrowBasePx / 2, 0f)
                            lineTo(indicatorArrowStartOffsetPx, indicatorArrowHeightPx)

                            lineTo(indicatorCornerRadiusPx, indicatorArrowHeightPx)
                            quadraticBezierTo(
                                0f,
                                indicatorArrowHeightPx,
                                0f,
                                indicatorArrowHeightPx + indicatorCornerRadiusPx,
                            )
                        } else {
                            lineTo(size.width, indicatorCornerRadiusPx + indicatorArrowHeightPx)
                            quadraticBezierTo(
                                size.width,
                                indicatorArrowHeightPx,
                                size.width - indicatorCornerRadiusPx,
                                indicatorArrowHeightPx,
                            )

                            val indicatorEnd = size.width - indicatorArrowStartOffsetPx
                            lineTo(indicatorEnd, indicatorArrowHeightPx)
                            lineTo(indicatorEnd - indicatorArrowBasePx / 2, 0f)
                            lineTo(indicatorEnd - indicatorArrowBasePx, indicatorArrowHeightPx)

                            lineTo(cornerRadiusPx, indicatorArrowHeightPx)
                            quadraticBezierTo(
                                0f,
                                indicatorArrowHeightPx,
                                0f,
                                indicatorArrowHeightPx + cornerRadiusPx,
                            )
                        }

                        close()
                    },
                )
            }
            CFRPopup.IndicatorDirection.DOWN -> {
                val messageBodyHeightPx = size.height - indicatorArrowHeightPx

                Outline.Generic(
                    path = Path().apply {
                        reset()

                        if (layoutDirection == LayoutDirection.Ltr) {
                            lineTo(0f, messageBodyHeightPx - indicatorCornerRadiusPx)
                            quadraticBezierTo(
                                0f,
                                size.height - indicatorArrowHeightPx,
                                indicatorCornerRadiusPx,
                                size.height - indicatorArrowHeightPx,
                            )

                            lineTo(indicatorArrowStartOffsetPx, messageBodyHeightPx)
                            lineTo(indicatorArrowStartOffsetPx + indicatorArrowBasePx / 2, size.height)
                            lineTo(indicatorArrowStartOffsetPx + indicatorArrowBasePx, messageBodyHeightPx)

                            lineTo(size.width - cornerRadiusPx, messageBodyHeightPx)
                            quadraticBezierTo(
                                size.width,
                                messageBodyHeightPx,
                                size.width,
                                messageBodyHeightPx - cornerRadiusPx,
                            )
                        } else {
                            lineTo(0f, messageBodyHeightPx - cornerRadiusPx)
                            quadraticBezierTo(
                                0f,
                                messageBodyHeightPx,
                                cornerRadiusPx,
                                messageBodyHeightPx,
                            )

                            val indicatorStartPx = size.width - indicatorArrowStartOffsetPx - indicatorArrowBasePx
                            lineTo(indicatorStartPx, messageBodyHeightPx)
                            lineTo(indicatorStartPx + indicatorArrowBasePx / 2, size.height)
                            lineTo(indicatorStartPx + indicatorArrowBasePx, messageBodyHeightPx)

                            lineTo(size.width - indicatorCornerRadiusPx, messageBodyHeightPx)
                            quadraticBezierTo(
                                size.width,
                                messageBodyHeightPx,
                                size.width,
                                messageBodyHeightPx - indicatorCornerRadiusPx,
                            )
                        }

                        lineTo(size.width, cornerRadiusPx)
                        quadraticBezierTo(
                            size.width,
                            0f,
                            size.width - cornerRadiusPx,
                            0f,
                        )

                        lineTo(cornerRadiusPx, 0f)
                        quadraticBezierTo(
                            0f,
                            0f,
                            0f,
                            cornerRadiusPx,
                        )

                        close()
                    },
                )
            }
        }
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

@Composable
@Preview(locale = "en", name = "LTR")
@Preview(locale = "ar", name = "RTL")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, name = "Dark theme")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO, name = "Light theme")
private fun CFRPopupBelowShapePreview() {
    Box(
        modifier = Modifier
            .height(100.dp)
            .width(200.dp)
            .background(
                shape = CFRPopupShape(UP, 10.dp, 10.dp, 10.dp),
                brush = Brush.linearGradient(
                    colors = listOf(Color.Cyan, Color.Blue),
                    end = Offset(0f, Float.POSITIVE_INFINITY),
                    start = Offset(Float.POSITIVE_INFINITY, 0f),
                ),
            ),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = "This is just a test",
            color = MaterialTheme.colors.onPrimary,
        )
    }
}

@Composable
@Preview(locale = "en", name = "LTR")
@Preview(locale = "ar", name = "RTL")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, name = "Dark theme")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO, name = "Light theme")
private fun CFRPopupAboveShapePreview() {
    Box(
        modifier = Modifier
            .height(100.dp)
            .width(200.dp)
            .background(
                shape = CFRPopupShape(DOWN, 10.dp, 10.dp, 10.dp),
                brush = Brush.linearGradient(
                    colors = listOf(Color.Cyan, Color.Blue),
                    end = Offset(0f, Float.POSITIVE_INFINITY),
                    start = Offset(Float.POSITIVE_INFINITY, 0f),
                ),
            ),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = "This is just a test",
            color = MaterialTheme.colors.onPrimary,
        )
    }
}
