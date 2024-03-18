/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.automatic

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.Locale

/**
 * Automatic Translate preference screen.
 *
 * @param automaticTranslationListPreferences List of [AutomaticTranslationItemPreference]s to display.
 * @param onItemClick Invoked when the user clicks on the a item from the list.
 */
@Composable
fun AutomaticTranslationPreference(
    automaticTranslationListPreferences: List<AutomaticTranslationItemPreference>,
    onItemClick: (AutomaticTranslationItemPreference) -> Unit,
) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer1,
            ),
    ) {
        TextListItem(
            label = stringResource(R.string.automatic_translation_header_preference),
            modifier = Modifier
                .padding(
                    start = 56.dp,
                )
                .semantics { heading() },
            maxLabelLines = Int.MAX_VALUE,
        )

        LazyColumn {
            items(automaticTranslationListPreferences) { item: AutomaticTranslationItemPreference ->
                var description: String? = null
                if (
                    item.automaticTranslationOptionPreference !is
                    AutomaticTranslationOptionPreference.OfferToTranslate
                ) {
                    description = stringResource(item.automaticTranslationOptionPreference.titleId)
                }
                TextListItem(
                    label = item.displayName,
                    description = description,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 56.dp),
                    onClick = {
                        onItemClick(item)
                    },
                )
            }
        }
    }
}

@Composable
internal fun getAutomaticTranslationListPreferences(): List<AutomaticTranslationItemPreference> {
    return mutableListOf<AutomaticTranslationItemPreference>().apply {
        add(
            AutomaticTranslationItemPreference(
                displayName = Locale.ENGLISH.displayLanguage,
                automaticTranslationOptionPreference = AutomaticTranslationOptionPreference.AlwaysTranslate(),
            ),
        )
        add(
            AutomaticTranslationItemPreference(
                displayName = Locale.FRENCH.displayLanguage,
                automaticTranslationOptionPreference = AutomaticTranslationOptionPreference.OfferToTranslate(),
            ),
        )
        add(
            AutomaticTranslationItemPreference(
                displayName = Locale.GERMAN.displayLanguage,
                automaticTranslationOptionPreference = AutomaticTranslationOptionPreference.NeverTranslate(),
            ),
        )
        add(
            AutomaticTranslationItemPreference(
                displayName = Locale.ITALIAN.displayLanguage,
                automaticTranslationOptionPreference = AutomaticTranslationOptionPreference.AlwaysTranslate(),
            ),
        )
    }
}

@Composable
@LightDarkPreview
private fun AutomaticTranslationPreferencePreview() {
    FirefoxTheme {
        AutomaticTranslationPreference(
            automaticTranslationListPreferences = getAutomaticTranslationListPreferences(),
            onItemClick = {},
        )
    }
}
