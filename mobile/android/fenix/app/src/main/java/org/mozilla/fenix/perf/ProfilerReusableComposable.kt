/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.annotation.StringRes
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.CircularProgressIndicator
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.RadioButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Dialogue top level card for the profiler.
 */
@Composable
fun ProfilerDialogueCard(content: @Composable () -> Unit) {
    Card(
        elevation = 8.dp,
        shape = RoundedCornerShape(12.dp),
    ) {
        content()
    }
}

/**
 * Top level radio button for the profiler dialogue.
 *
 * @param text The main text to be displayed.
 * @param subText The subtext to be displayed.
 * @param selected [Boolean] that indicates whether the radio button is currently selected.
 * @param onClick Invoked when the radio button is clicked.
 */
@Composable
fun ProfilerLabeledRadioButton(
    text: String,
    subText: String,
    selected: Boolean = false,
    onClick: () -> Unit,
) {
    Row(
        modifier = Modifier.clickable { onClick() },
    ) {
        RadioButton(
            selected = selected,
            onClick = {},
            enabled = true,
        )
        Column(
            modifier = Modifier.padding(horizontal = 8.dp),
        ) {
            Text(text = text)
            Text(
                text = subText,
                fontWeight = FontWeight.ExtraLight,
            )
        }
    }
}

/**
 * Profiler Dialogue to display circular spinner when waiting.
 */
@Composable
fun WaitForProfilerDialog(
    @StringRes message: Int,
) {
    ProfilerDialogueCard {
        Column(
            modifier = Modifier.padding(8.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                text = stringResource(message),
                fontWeight = FontWeight.Bold,
                fontSize = 15.sp,
                modifier = Modifier.padding(8.dp),
            )
            Spacer(modifier = Modifier.height(2.dp))
            CircularProgressIndicator()
        }
    }
}

/**
 * Preview example of [ProfilerLabeledRadioButton].
 */
@Composable
@LightDarkPreview
private fun ProfilerLabeledRadioButtonPreview() {
    val radioOptions = listOf("Firefox", "Graphics", "Media", "Networking")
    val selectedOption = remember { mutableStateOf("Firefox") }

    FirefoxTheme {
        Column(
            modifier = Modifier.background(FirefoxTheme.colors.layer1),
        ) {
            radioOptions.forEach { text ->
                ProfilerLabeledRadioButton(
                    text = text,
                    subText = "Sub",
                    selected = selectedOption.value == text,
                    onClick = {
                        selectedOption.value = text
                    },
                )
            }
        }
    }
}
