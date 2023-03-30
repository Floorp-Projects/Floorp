/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import android.content.Context
import android.content.res.Configuration
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Paint
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.R.drawable
import org.mozilla.fenix.R.string
import org.mozilla.fenix.compose.DropdownMenu
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.list.ExpandableListHeader
import org.mozilla.fenix.ext.getIconColor
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Rectangular shape with all corners rounded used to display a collapsed collection.
 */
private val collapsedCollectionShape = RoundedCornerShape(8.dp)

/**
 * Rectangular shape with only the top corners rounded used to display an expanded collection with other views
 * placed immediately below this which can be shown immediately next to it, with no visible separation.
 */
private val expandedCollectionShape = RoundedCornerShape(topStart = 8.dp, topEnd = 8.dp)

/**
 * Displays an individual [TabCollection].
 *
 * @param collection [TabCollection] to display.
 * @param expanded Whether the collection is expanded to show it's containing tabs or not.
 * @param menuItems List of [MenuItem] to be shown in a menu.
 * @param onToggleCollectionExpanded Invoked when the user clicks on the collection.
 * @param onCollectionShareTabsClicked Invoked when the user clicks to share the collection.
 */
@Composable
@Suppress("LongMethod")
fun Collection(
    collection: TabCollection,
    expanded: Boolean,
    menuItems: List<MenuItem>,
    onToggleCollectionExpanded: (TabCollection, Boolean) -> Unit,
    onCollectionShareTabsClicked: (TabCollection) -> Unit,
) {
    var isMenuExpanded by remember(collection) { mutableStateOf(false) }
    val isExpanded by remember(collection) { mutableStateOf(expanded) }

    Card(
        modifier = Modifier
            .semantics(mergeDescendants = true) {}
            .clickable(
                onClickLabel = if (isExpanded) {
                    stringResource(R.string.a11y_action_label_collapse)
                } else {
                    stringResource(R.string.a11y_action_label_expand)
                },
                onClick = { onToggleCollectionExpanded(collection, !isExpanded) },
            )
            .height(48.dp),
        shape = if (isExpanded) expandedCollectionShape else collapsedCollectionShape,
        backgroundColor = FirefoxTheme.colors.layer2,
        elevation = 5.dp,
    ) {
        Row(
            modifier = Modifier
                .fillMaxHeight(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                painter = painterResource(drawable.ic_tab_collection),
                contentDescription = null,
                modifier = Modifier.padding(
                    start = 16.dp,
                    end = 8.dp, // (24.dp - 16.dp) hardcoded in ExpandableListHeader
                ),
                tint = Paint().apply {
                    color = Color(collection.getIconColor(LocalContext.current))
                    blendMode = BlendMode.SrcIn
                }.color,
            )

            ExpandableListHeader(
                headerText = collection.title,
                headerTextStyle = FirefoxTheme.typography.headline7,
                expanded = isExpanded,
            ) {
                if (isExpanded) {
                    Row {
                        IconButton(
                            onClick = { onCollectionShareTabsClicked(collection) },
                        ) {
                            Icon(
                                painter = painterResource(drawable.ic_share),
                                contentDescription = stringResource(string.share_button_content_description),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        }

                        IconButton(
                            onClick = {
                                isMenuExpanded = !isMenuExpanded
                            },
                        ) {
                            Icon(
                                painter = painterResource(drawable.ic_menu),
                                contentDescription = stringResource(
                                    string.collection_menu_button_content_description,
                                ),
                                tint = FirefoxTheme.colors.iconPrimary,
                            )

                            DropdownMenu(
                                showMenu = isMenuExpanded,
                                menuItems = menuItems,
                                onDismissRequest = { isMenuExpanded = false },
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun CollectionDarkPreview() {
    FirefoxTheme(Theme.Dark) {
        Collection(
            collection = collectionPreview,
            expanded = false,
            menuItems = emptyList(),
            onToggleCollectionExpanded = { _, _ -> },
            onCollectionShareTabsClicked = {},
        )
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun CollectionDarkExpandedPreview() {
    FirefoxTheme(Theme.Dark) {
        Collection(
            collection = collectionPreview,
            expanded = true,
            menuItems = emptyList(),
            onToggleCollectionExpanded = { _, _ -> },
            onCollectionShareTabsClicked = {},
        )
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CollectionLightPreview() {
    FirefoxTheme(Theme.Light) {
        Collection(
            collection = collectionPreview,
            expanded = false,
            menuItems = emptyList(),
            onToggleCollectionExpanded = { _, _ -> },
            onCollectionShareTabsClicked = {},
        )
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CollectionLightExpandedPreview() {
    FirefoxTheme(Theme.Light) {
        Collection(
            collection = collectionPreview,
            expanded = true,
            menuItems = emptyList(),
            onToggleCollectionExpanded = { _, _ -> },
            onCollectionShareTabsClicked = {},
        )
    }
}

private val collectionPreview = object : TabCollection {
    override val id: Long = 1L
    override val tabs: List<Tab> = emptyList()
    override val title: String = "Collection 1"

    override fun restore(
        context: Context,
        engine: Engine,
        restoreSessionId: Boolean,
    ): List<RecoverableTab> = emptyList()

    override fun restoreSubset(
        context: Context,
        engine: Engine,
        tabs: List<Tab>,
        restoreSessionId: Boolean,
    ): List<RecoverableTab> = emptyList()
}
