/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.cfr

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.R.drawable
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Complete content of the popup.
 * [CFRPopupShape] with a gradient background containing [text] and a dismiss ("X") button.
 *
 * @param text String message in the popup.
 * @param indicatorDirection The direction the indicator arrow is pointing to.
 * @param indicatorArrowStartOffset Maximum distance between the popup start and the indicator arrow.
 * If there isn't enough space this could automatically be overridden up to 0.
 * @param onDismiss  Callback for when the popup is dismissed indicating also if the dismissal
 * was explicit - by tapping the "X" button or not.
 * @param action Optional other composable to show just below the popup text.
 */
@Composable
@Suppress("LongParameterList", "LongMethod")
fun CFRPopupContent(
    text: String,
    indicatorDirection: CFRPopup.IndicatorDirection,
    indicatorArrowStartOffset: Dp,
    onDismiss: (Boolean) -> Unit,
    popupWidth: Dp = CFRPopup.DEFAULT_WIDTH.dp,
    action: @Composable (() -> Unit) = {}
) {
    val popupShape = CFRPopupShape(
        indicatorDirection,
        indicatorArrowStartOffset,
        CFRPopup.DEFAULT_INDICATOR_HEIGHT.dp,
        CFRPopup.DEFAULT_CORNER_RADIUS.dp,
    )

    Box(modifier = Modifier.width(popupWidth + CFRPopup.DEFAULT_HORIZONTAL_PADDING.dp * 2)) {
        Surface(
            color = Color.Transparent,
            // Need to override the default RectangleShape to avoid casting shadows for that shape.
            shape = popupShape,
            modifier = Modifier
                .align(Alignment.Center)
                .background(
                    shape = popupShape,
                    brush = Brush.linearGradient(
                        colors = listOf(
                            FirefoxTheme.colors.gradientEnd,
                            FirefoxTheme.colors.gradientStart
                        ),
                        end = Offset(0f, Float.POSITIVE_INFINITY),
                        start = Offset(Float.POSITIVE_INFINITY, 0f)
                    )
                )
                .wrapContentHeight()
                .width(popupWidth)
        ) {

            Column(
                modifier = Modifier
                    .padding(
                        start = 16.dp,
                        top = 16.dp + if (indicatorDirection == CFRPopup.IndicatorDirection.UP) {
                            CFRPopup.DEFAULT_INDICATOR_HEIGHT.dp
                        } else {
                            0.dp
                        },
                        end = 16.dp,
                        bottom = 16.dp +
                            if (indicatorDirection == CFRPopup.IndicatorDirection.DOWN) {
                                CFRPopup.DEFAULT_INDICATOR_HEIGHT.dp
                            } else {
                                0.dp
                            }
                    )
            ) {
                Text(
                    text = text,
                    modifier = Modifier.padding(
                        end = 24.dp, // 8.dp extra padding to the "X" icon
                    ),
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.body2
                )

                action()
            }
        }

        IconButton(
            onClick = { onDismiss(true) },
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(
                    top = if (indicatorDirection == CFRPopup.IndicatorDirection.UP) 14.dp else 0.dp,
                    end = 6.dp
                )
                .size(48.dp)
        ) {
            Icon(
                painter = painterResource(drawable.mozac_ic_close_20),
                contentDescription = stringResource(R.string.cfr_dismiss_button_default_content_description),
                modifier = Modifier
                    // Following alignment and padding are necessary to visually align the middle
                    // of the "X" button with the top of the text.
                    .align(Alignment.TopCenter)
                    .padding(top = 10.dp)
                    .size(24.dp),
                tint = FirefoxTheme.colors.iconOnColor
            )
        }
    }
}

@Composable
@Preview(locale = "en", name = "LTR")
@Preview(locale = "ar", name = "RTL")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, name = "Dark theme")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO, name = "Light theme")
private fun CFRPopupAbovePreview() {
    FirefoxTheme {
        CFRPopupContent(
            text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt",
            indicatorDirection = CFRPopup.IndicatorDirection.DOWN,
            indicatorArrowStartOffset = CFRPopup.DEFAULT_INDICATOR_START_OFFSET.dp,
            onDismiss = { }
        )
    }
}

@Composable
@Preview(locale = "en", name = "LTR")
@Preview(locale = "ar", name = "RTL")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, name = "Dark theme")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO, name = "Light theme")
private fun CFRPopupBelowPreview() {
    FirefoxTheme {
        CFRPopupContent(
            text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt",
            indicatorDirection = CFRPopup.IndicatorDirection.UP,
            indicatorArrowStartOffset = CFRPopup.DEFAULT_INDICATOR_START_OFFSET.dp,
            onDismiss = { }
        )
    }
}
