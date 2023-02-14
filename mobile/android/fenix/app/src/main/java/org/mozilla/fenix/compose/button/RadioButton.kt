/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.button

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.material.RadioButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Radio Button.
 *
 * @param selected [Boolean] indicating whether the radio button is selected or not.
 * @param modifier [Modifier] to be applied to the radio button.
 * @param enabled [Boolean] that controls if radio button is selectable.
 * @param onClick Invoked when the radio button is clicked.
 */
@Composable
fun RadioButton(
    selected: Boolean,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    androidx.compose.material.RadioButton(
        selected = selected,
        modifier = modifier,
        enabled = enabled,
        colors = RadioButtonDefaults.colors(
            selectedColor = FirefoxTheme.colors.formSelected,
            unselectedColor = FirefoxTheme.colors.formDefault,
            disabledColor = FirefoxTheme.colors.formDisabled,
        ),
        onClick = onClick,
    )
}

@Composable
@LightDarkPreview
private fun RadioButtonPreview() {
    val radioOptions = listOf("One", "Two", "Three")
    val (selectedOption, onOptionSelected) = remember { mutableStateOf(radioOptions[1]) }

    FirefoxTheme {
        Column(
            modifier = Modifier.background(FirefoxTheme.colors.layer1),
        ) {
            radioOptions.forEach { text ->
                val selected = text == selectedOption

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp)
                        .selectable(
                            selected = selected,
                            onClick = { onOptionSelected(text) },
                        ),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    RadioButton(
                        selected = selected,
                        onClick = { onOptionSelected(text) },
                    )

                    Spacer(modifier = Modifier.padding(16.dp))

                    Text(text = text)
                }
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .selectable(
                        selected = false,
                        onClick = {},
                    )
                    .padding(horizontal = 16.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                RadioButton(
                    selected = false,
                    enabled = false,
                    onClick = {},
                )

                Spacer(modifier = Modifier.padding(16.dp))

                Text(text = "Disabled")
            }
        }
    }
}
