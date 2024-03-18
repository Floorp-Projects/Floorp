/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * [RecyclerView.ViewHolder] for displaying an individual [TabCollection].
 * Clients are expected to use [bindSession] to link a particular [TabCollection] to be displayed
 * otherwise this will be an empty, 0 size View.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @param interactor [CollectionInteractor] callback for user interactions.
 */
class CollectionViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: CollectionInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {
    private var collectionData = CollectionInfo()

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    @Composable
    override fun Content() {
        val collectionInfo by remember { mutableStateOf(collectionData) }

        collectionInfo.collection?.let { collection ->
            val menuItems = getMenuItems(
                collection = collection,
                onOpenTabsTapped = interactor::onCollectionOpenTabsTapped,
                onRenameCollectionTapped = interactor::onRenameCollectionTapped,
                onAddTabTapped = interactor::onCollectionAddTabTapped,
                onDeleteCollectionTapped = interactor::onDeleteCollectionTapped,
            )

            Column {
                Spacer(Modifier.height(12.dp))

                Collection(
                    collection = collection,
                    expanded = collectionInfo.isExpanded,
                    menuItems = menuItems,
                    onToggleCollectionExpanded = interactor::onToggleCollectionExpanded,
                    onCollectionShareTabsClicked = interactor::onCollectionShareTabsClicked,
                )
            }
        }
    }

    /**
     * Dynamically replace the current [TabCollection] shown in this `ViewHolder`.
     *
     * @param collection [TabCollection] to be shown
     * @param expanded Whether to show the collection as expanded or collapsed.
     */
    fun bindSession(collection: TabCollection, expanded: Boolean) {
        collectionData = CollectionInfo(
            collection = collection,
            isExpanded = expanded,
        )
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

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
private fun getMenuItems(
    collection: TabCollection,
    onOpenTabsTapped: (TabCollection) -> Unit,
    onRenameCollectionTapped: (TabCollection) -> Unit,
    onAddTabTapped: (TabCollection) -> Unit,
    onDeleteCollectionTapped: (TabCollection) -> Unit,
): List<MenuItem> {
    return listOfNotNull(
        MenuItem(
            title = stringResource(R.string.collection_open_tabs),
            color = FirefoxTheme.colors.textPrimary,
        ) {
            onOpenTabsTapped(collection)
        },
        MenuItem(
            title = stringResource(R.string.collection_rename),
            color = FirefoxTheme.colors.textPrimary,
        ) {
            onRenameCollectionTapped(collection)
        },

        if (LocalContext.current.components.core.store.state.normalTabs.isNotEmpty()) {
            MenuItem(
                title = stringResource(R.string.add_tab),
                color = FirefoxTheme.colors.textPrimary,
            ) {
                onAddTabTapped(collection)
            }
        } else {
            null
        },

        MenuItem(
            title = stringResource(R.string.collection_delete),
            color = FirefoxTheme.colors.textWarning,
        ) {
            onDeleteCollectionTapped(collection)
        },
    )
}

/**
 * Wrapper over a [TabCollection] adding information about whether it should be shown as expanded or collapsed.
 *
 * @property collection [TabCollection] to display.
 * @property isExpanded Whether the collection is expanded to show it's containing tabs or not.
 */
@Stable
private data class CollectionInfo(
    val collection: TabCollection? = null,
    val isExpanded: Boolean = false,
)
