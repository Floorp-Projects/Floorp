/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Button
import androidx.compose.material.CircularProgressIndicator
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private val ProgressIndicatorWidth = 5.dp

/**
 * UI displaying a circular progress indicator with a determinate value.
 *
 * @param progress The progress value to display.
 * @param modifier [Modifier] to be applied to the indicator.
 */
@Composable
fun DeterminateProgressIndicator(
    progress: Float,
    modifier: Modifier = Modifier,
) {
    val floatState = animateFloatAsState(
        progress,
        label = "DeterminateProgressIndicator",
    )

    CircularProgressIndicator(
        modifier = modifier,
        progress = floatState.value,
        color = FirefoxTheme.colors.layerAccent,
        backgroundColor = FirefoxTheme.colors.actionTertiary,
        strokeWidth = ProgressIndicatorWidth,
        strokeCap = StrokeCap.Butt,
    )
}

@LightDarkPreview
@Composable
private fun DeterminateProgressIndicatorPreviewDark() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            var progress: Float by remember { mutableFloatStateOf(0f) }

            DeterminateProgressIndicator(
                progress = progress,
                modifier = Modifier.size(48.dp),
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Button(
                    onClick = { progress += 0.1f },
                    modifier = Modifier.height(56.dp),
                ) {
                    Text(text = "Increase progress")
                }

                Button(
                    onClick = { progress -= 0.1f },
                    modifier = Modifier.height(56.dp),
                ) {
                    Text(text = "Decrease progress")
                }
            }
        }
    }
}
