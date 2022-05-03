/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.Spacer
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.ext.dashedBorder
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * [CollectionsPlaceholder] for displaying a message detailing the collections feature and
 * allowing users to easily start creating their collection.
 *
 * @param showAddToCollectionButton Whether or not the "Add to Collection" button should be shown.
 * @param onAddTabsToCollectionButtonClick Invoked when the user clicks on the "Add Tabs to Collection" button.
 * @param onRemovePlaceholderClick Invoked when the user clicks on the close button to remove the Collections
 * placeholder.
 */
@Composable
fun CollectionsPlaceholder(
    showAddToCollectionButton: Boolean,
    onAddTabsToCollectionButtonClick: () -> Unit,
    onRemovePlaceholderClick: () -> Unit,
) {
    Box(
        modifier = Modifier
            .semantics(mergeDescendants = true) {}
            .dashedBorder(
                color = FirefoxTheme.colors.borderPrimary,
                cornerRadius = 8.dp,
                dashHeight = 2.dp,
                dashWidth = 4.dp
            )
    ) {
        Column(
            Modifier
                .padding(16.dp)
                .fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
            ) {
                SectionHeader(
                    text = stringResource(R.string.collections_header),
                    modifier = Modifier.weight(1f)
                )

                IconButton(
                    onClick = onRemovePlaceholderClick,
                    modifier = Modifier.size(20.dp),
                ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_close_20),
                        contentDescription = stringResource(
                            R.string.remove_home_collection_placeholder_content_description
                        ),
                        tint = FirefoxTheme.colors.iconPrimary
                    )
                }
            }

            Spacer(modifier = Modifier.height(4.dp))

            SecondaryText(
                text = stringResource(R.string.no_collections_description2),
                modifier = Modifier.fillMaxWidth(),
                fontSize = 14.sp
            )

            if (showAddToCollectionButton) {
                Spacer(modifier = Modifier.height(12.dp))

                PrimaryButton(
                    text = stringResource(R.string.tabs_menu_save_to_collection1),
                    icon = painterResource(R.drawable.ic_tab_collection),
                    onClick = onAddTabsToCollectionButtonClick
                )
            }
        }
    }
}

@Composable
@Preview
private fun CollectionsPlaceholderPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            CollectionsPlaceholder(
                showAddToCollectionButton = true,
                onAddTabsToCollectionButtonClick = {},
                onRemovePlaceholderClick = {}
            )
        }
    }
}
