/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.nevertranslatesite

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Never Translate Site preference screen.
 *
 * @param neverTranslateSiteListPreferences List of [NeverTranslateSiteListItemPreference]s to display.
 * @param onItemClick Invoked when the user clicks on the a item from the list.
 */
@Composable
fun NeverTranslateSitePreference(
    neverTranslateSiteListPreferences: List<NeverTranslateSiteListItemPreference>,
    onItemClick: (NeverTranslateSiteListItemPreference) -> Unit,
) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer1,
            ),
    ) {
        TextListItem(
            label = stringResource(R.string.never_translate_site_header_preference),
            modifier = Modifier
                .padding(
                    start = 56.dp,
                )
                .semantics { heading() },
            maxLabelLines = Int.MAX_VALUE,
        )

        LazyColumn {
            items(neverTranslateSiteListPreferences) { item: NeverTranslateSiteListItemPreference ->
                val itemContentDescription = stringResource(
                    id = R.string.never_translate_site_item_list_content_description_preference,
                    item.websiteUrl,
                )
                TextListItem(
                    label = item.websiteUrl,
                    modifier = Modifier
                        .padding(
                            start = 56.dp,
                        )
                        .clearAndSetSemantics {
                            role = Role.Button
                            contentDescription = itemContentDescription
                        },
                    onClick = { onItemClick(item) },
                    iconPainter = painterResource(R.drawable.mozac_ic_delete_24),
                    onIconClick = { onItemClick(item) },
                )
            }
        }
    }
}

@Composable
internal fun getNeverTranslateListItemsPreference(): List<NeverTranslateSiteListItemPreference> {
    return mutableListOf<NeverTranslateSiteListItemPreference>().apply {
        add(
            NeverTranslateSiteListItemPreference(
                websiteUrl = "mozilla.org",
            ),
        )
    }
}

@Composable
@LightDarkPreview
private fun NeverTranslateSitePreferencePreview() {
    FirefoxTheme {
        NeverTranslateSitePreference(
            neverTranslateSiteListPreferences = getNeverTranslateListItemsPreference(),
        ) {}
    }
}
