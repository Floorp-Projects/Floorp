/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import android.content.Context
import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.Immutable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R.string
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Menu shown for a [org.mozilla.fenix.home.collections.Collection].
 *
 * @see [DropdownMenu]
 *
 * @param showMenu Whether this is currently open and visible to the user.
 * @param menuItems List of options shown.
 * @param onDismissRequest Called when the user chooses a menu option or requests to dismiss the menu.
 */
@Composable
fun CollectionMenu(
    showMenu: Boolean,
    menuItems: List<CollectionMenuItem>,
    onDismissRequest: () -> Unit,
) {
    DisposableEffect(LocalConfiguration.current.orientation) {
        onDispose { onDismissRequest() }
    }

    // DropdownMenu uses the medium shape from MaterialTheme.
    // Override it's corner radius to be the same 8.dp as in mozac_browser_menu_corner_radius
    MaterialTheme(shapes = MaterialTheme.shapes.copy(medium = RoundedCornerShape(8.dp))) {
        DropdownMenu(
            expanded = showMenu,
            onDismissRequest = { onDismissRequest() },
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer2),
        ) {
            for (item in menuItems) {
                DropdownMenuItem(
                    onClick = {
                        onDismissRequest()
                        item.onClick()
                    },
                ) {
                    Text(
                        text = item.title,
                        color = item.color,
                        maxLines = 1,
                        modifier = Modifier
                            .fillMaxHeight()
                            .align(Alignment.CenterVertically)
                    )
                }
            }
        }
    }
}

/**
 * A menu item for collections.
 *
 * @property title The menu item title.
 * @property color The color that should be set for the title.
 * @property onClick Invoked when the user clicks on the menu item.
 */
@Immutable
data class CollectionMenuItem(
    val title: String,
    val color: Color,
    val onClick: () -> Unit,
)

/**
 * Constructs and returns the default list of menu options for a [TabCollection].
 *
 * @param collection [TabCollection] for which the menu will be shown.
 * Might serve as an argument for the callbacks for when the user interacts with certain menu options.
 * @param onOpenTabsTapped Invoked when the user chooses to open the tabs from [collection].
 * @param onRenameCollectionTapped Invoked when the user chooses to rename the [collection].
 * @param onAddTabTapped Invoked when the user chooses to add tabs to [collection].
 * @param onDeleteCollectionTapped Invoked when the user chooses to delete [collection].
 */
@Composable
fun getMenuItems(
    collection: TabCollection,
    onOpenTabsTapped: (TabCollection) -> Unit,
    onRenameCollectionTapped: (TabCollection) -> Unit,
    onAddTabTapped: (TabCollection) -> Unit,
    onDeleteCollectionTapped: (TabCollection) -> Unit,
): List<CollectionMenuItem> {
    return listOfNotNull(
        CollectionMenuItem(
            title = stringResource(string.collection_open_tabs),
            color = FirefoxTheme.colors.textPrimary
        ) {
            onOpenTabsTapped(collection)
        },
        CollectionMenuItem(
            title = stringResource(string.collection_rename),
            color = FirefoxTheme.colors.textPrimary
        ) {
            onRenameCollectionTapped(collection)
        },

        if (hasOpenTabs()) {
            CollectionMenuItem(
                title = stringResource(string.add_tab),
                color = FirefoxTheme.colors.textPrimary
            ) {
                onAddTabTapped(collection)
            }
        } else {
            null
        },

        CollectionMenuItem(
            title = stringResource(string.collection_delete),
            color = FirefoxTheme.colors.textWarning
        ) {
            onDeleteCollectionTapped(collection)
        },
    )
}

@Composable
private fun hasOpenTabs() = when (inComposePreview) {
    true -> true
    false -> LocalContext.current.components.core.store.state.normalTabs.isNotEmpty()
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun CollectionMenuDarkPreview() {
    FirefoxTheme(Theme.Dark) {
        CollectionMenu(
            showMenu = true,
            menuItems = getMenuItems(
                collection = collectionPreview,
                onOpenTabsTapped = {},
                onRenameCollectionTapped = {},
                onAddTabTapped = {},
                onDeleteCollectionTapped = {}
            ),
        ) {}
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CollectionMenuLightPreview() {
    FirefoxTheme(Theme.Light) {
        CollectionMenu(
            showMenu = true,
            menuItems = getMenuItems(
                collection = collectionPreview,
                onOpenTabsTapped = {},
                onRenameCollectionTapped = {},
                onAddTabTapped = {},
                onDeleteCollectionTapped = {}
            ),
        ) {}
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
