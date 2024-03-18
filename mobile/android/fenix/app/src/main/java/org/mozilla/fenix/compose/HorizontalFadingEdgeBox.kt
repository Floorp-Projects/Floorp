/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Displays the [content] with the right edge fading.
 *
 * @param modifier [Modifier] for the container.
 * @param fadeWidth Length of the fading edge.
 * @param backgroundColor [Color] of the background shown under the content.
 * @param isContentRtl Whether or not the content should be displayed Right to Left
 * @param content The content whose right edge must be faded.
 */
@Composable
fun HorizontalFadingEdgeBox(
    modifier: Modifier = Modifier,
    fadeWidth: Dp = 25.dp,
    backgroundColor: Color = Color.Transparent,
    isContentRtl: Boolean = false,
    content: @Composable BoxScope.() -> Unit,
) {
    // List of colors defining the direction of the fade effect
    val colorList = listOf(Color.Transparent, backgroundColor)

    CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Ltr) {
        Box(modifier) {
            content()
            Spacer(
                Modifier
                    .width(fadeWidth)
                    .fillMaxHeight()
                    .align(
                        if (isContentRtl) {
                            Alignment.CenterStart
                        } else {
                            Alignment.CenterEnd
                        },
                    )
                    .background(
                        Brush.horizontalGradient(
                            colors = if (isContentRtl) {
                                colorList.reversed()
                            } else {
                                colorList
                            },
                        ),
                    ),
            )
        }
    }
}

@Preview
@Composable
private fun FadingRightTextPreview() {
    FirefoxTheme {
        Surface(modifier = Modifier.background(FirefoxTheme.colors.layer1)) {
            HorizontalFadingEdgeBox(
                modifier = Modifier
                    .width(250.dp)
                    .height(20.dp)
                    .clipToBounds(),
                backgroundColor = FirefoxTheme.colors.layer1,
            ) {
                Text(
                    "Example text set to fade on the right",
                    modifier = Modifier
                        .fillMaxSize(),
                    softWrap = false,
                )
            }
        }
    }
}

@Preview
@Composable
private fun FadingLeftTextPreview() {
    FirefoxTheme {
        Surface(modifier = Modifier.background(FirefoxTheme.colors.layer1)) {
            HorizontalFadingEdgeBox(
                modifier = Modifier
                    .width(250.dp)
                    .height(20.dp)
                    .clipToBounds(),
                isContentRtl = true,
                fadeWidth = 50.dp,
                backgroundColor = FirefoxTheme.colors.layer1,
            ) {
                Text(
                    "Example text set to fade on the left",
                    modifier = Modifier
                        .fillMaxSize(),
                    softWrap = false,
                )
            }
        }
    }
}
