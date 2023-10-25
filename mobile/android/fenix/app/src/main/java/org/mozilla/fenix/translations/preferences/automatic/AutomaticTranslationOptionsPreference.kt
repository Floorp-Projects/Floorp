/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.automatic

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.RadioButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Firefox Automatic Translation Options preference screen.
 *
 * @param selectedOption Selected option that will come from the translations engine.
 */
@Composable
fun AutomaticTranslationOptionsPreference(
    selectedOption: AutomaticTranslationOptionPreference,
) {
    val optionsList = arrayListOf(
        AutomaticTranslationOptionPreference.OfferToTranslate(),
        AutomaticTranslationOptionPreference.AlwaysTranslate(),
        AutomaticTranslationOptionPreference.NeverTranslate(),
    )
    val selected = remember { mutableStateOf(selectedOption) }
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer1,
            ),
    ) {
        LazyColumn {
            items(optionsList) { item: AutomaticTranslationOptionPreference ->
                AutomaticTranslationOptionRadioButton(
                    text = stringResource(item.titleId),
                    subText = stringResource(
                        item.summaryId.first(),
                        stringResource(item.summaryId.last()),
                    ),
                    selected = selected.value == item,
                    onClick = {
                        selected.value = item
                    },
                )
            }
        }
    }
}

/**
 * Top level radio button for the automatic translation options.
 *
 * @param text The main text to be displayed.
 * @param subText The subtext to be displayed.
 * @param selected [Boolean] that indicates whether the radio button is currently selected.
 * @param onClick Invoked when the radio button is clicked.
 */
@Composable
fun AutomaticTranslationOptionRadioButton(
    text: String,
    subText: String,
    selected: Boolean = false,
    onClick: () -> Unit,
) {
    Row(
        modifier = Modifier
            .padding(start = 16.dp, top = 6.dp, bottom = 6.dp, end = 16.dp)
            .selectable(
                selected = selected,
                role = Role.RadioButton,
                onClick = { onClick() },
            ),
    ) {
        RadioButton(
            selected = selected,
            modifier = Modifier.clearAndSetSemantics {},
            onClick = {},
            enabled = true,
        )
        Column(
            modifier = Modifier.padding(start = 16.dp),
        ) {
            Text(
                text = text,
                color = FirefoxTheme.colors.textPrimary,
            )
            Text(
                text = subText,
                color = FirefoxTheme.colors.textSecondary,
                style = FirefoxTheme.typography.body2,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun AutomaticTranslationOptionsPreview() {
    FirefoxTheme {
        AutomaticTranslationOptionsPreference(
            selectedOption = AutomaticTranslationOptionPreference.AlwaysTranslate(),
        )
    }
}
