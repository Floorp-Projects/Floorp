/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.StartOffset
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private const val ANIMATION_DURATION_MS = 1000
private const val INITIAL_ALPHA = 0.25f
private const val TARGET_ALPHA = 1f
private val boxes = listOf(
    BoxInfo(height = 80.dp, offsetMillis = 500),
    BoxInfo(height = 80.dp, offsetMillis = 1000),
    BoxInfo(height = 192.dp, offsetMillis = 0),
    BoxInfo(height = 40.dp, offsetMillis = 500),
    BoxInfo(height = 192.dp, offsetMillis = 1000),
)

/**
 * Loading UI for review quality check content.
 */
@Composable
fun ProductReviewLoading(
    modifier: Modifier = Modifier,
) {
    val infiniteTransition = rememberInfiniteTransition("ProductReviewLoading")

    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        boxes.forEach { boxInfo ->
            val alpha by infiniteTransition.animateFloat(
                initialValue = INITIAL_ALPHA,
                targetValue = TARGET_ALPHA,
                animationSpec = infiniteRepeatable(
                    animation = tween(ANIMATION_DURATION_MS, easing = LinearEasing),
                    repeatMode = RepeatMode.Reverse,
                    initialStartOffset = StartOffset(offsetMillis = boxInfo.offsetMillis),
                ),
                label = "ProductReviewLoading-Alpha",
            )

            Box(
                modifier = Modifier
                    .height(boxInfo.height)
                    .fillMaxWidth()
                    .alpha(alpha)
                    .background(
                        color = FirefoxTheme.colors.layer3,
                        shape = RoundedCornerShape(8.dp),
                    ),
            )
        }
    }
}

private data class BoxInfo(
    val height: Dp,
    val offsetMillis: Int,
)

@LightDarkPreview
@Composable
private fun ProductReviewLoadingPreview() {
    FirefoxTheme {
        ReviewQualityCheckScaffold(
            onRequestDismiss = {},
        ) {
            ProductReviewLoading()
        }
    }
}
