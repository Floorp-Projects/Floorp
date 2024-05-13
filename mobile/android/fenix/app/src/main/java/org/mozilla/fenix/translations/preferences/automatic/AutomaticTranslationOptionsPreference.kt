/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.automatic

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.RadioButtonListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Firefox Automatic Translation Options preference screen.
 *
 * @param selectedOption Selected option that will come from the translations engine.
 * @param onItemClick Invoked when the user clicks on a [AutomaticTranslationOptionPreference] from the list.
 */
@Composable
fun AutomaticTranslationOptionsPreference(
    selectedOption: AutomaticTranslationOptionPreference,
    onItemClick: (AutomaticTranslationOptionPreference) -> Unit,
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
                RadioButtonListItem(
                    label = stringResource(item.titleId),
                    selected = selected.value == item,
                    modifier = Modifier
                        .defaultMinSize(minHeight = 76.dp)
                        .wrapContentHeight(),
                    description = stringResource(
                        item.summaryId.first(),
                        stringResource(item.summaryId.last()),
                    ),
                    maxDescriptionLines = Int.MAX_VALUE,
                    onClick = {
                        selected.value = item
                        onItemClick(item)
                    },
                )
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun AutomaticTranslationOptionsPreview() {
    FirefoxTheme {
        AutomaticTranslationOptionsPreference(
            selectedOption = AutomaticTranslationOptionPreference.AlwaysTranslate(),
            onItemClick = {},
        )
    }
}
