/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.annotation.StringRes
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
import androidx.compose.material.RadioButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * Dialogue top level card for the profiler
 */
@Composable
fun ProfilerDialogueCard(content: @Composable () -> Unit) {
    Card(
        elevation = 8.dp,
        shape = RoundedCornerShape(12.dp)
    ) {
        content()
    }
}

/**
 * Top level radio button for the profiler dialogue
 */
@Composable
fun ProfilerLabeledRadioButton(
    text: String,
    subText: String,
    state: MutableState<String>
) {
    Row {
        RadioButton(
            selected = state.value == text,
            onClick = { state.value = text },
            enabled = true,
        )
        Column {
            Text(
                text = text,
                modifier = Modifier
                    .padding(start = 8.dp)
                    .clickable {
                        state.value = text
                    }
            )
            Text(
                text = subText,
                fontWeight = FontWeight.ExtraLight,
                modifier = Modifier
                    .padding(start = 8.dp)
                    .clickable { state.value = text }
            )
        }
    }
}

/**
 * Profiler Dialogue to display circular spinner when waiting
 */
@Composable
fun WaitForProfilerDialog(
    @StringRes message: Int
) {
    ProfilerDialogueCard {
        Column(
            modifier = Modifier.padding(8.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = stringResource(message),
                fontWeight = FontWeight.Bold,
                fontSize = 15.sp,
                modifier = Modifier.padding(8.dp)
            )
            Spacer(modifier = Modifier.height(2.dp))
            CircularProgressIndicator()
        }
    }
}
