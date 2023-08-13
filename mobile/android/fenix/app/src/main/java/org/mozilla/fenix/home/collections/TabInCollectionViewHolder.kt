/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor

/**
 * [RecyclerView.ViewHolder] for displaying an individual [Tab].
 * Clients are expected to use [bindSession] to link a particular [Tab] to be displayed
 * otherwise this will be an empty, 0 size View.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @property interactor [CollectionInteractor] callback for user interactions.
 */
class TabInCollectionViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: CollectionInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {
    private var tabData = TabInfo()

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    @Composable
    override fun Content() {
        val tabInfo = remember { mutableStateOf(tabData) }

        tabInfo.value.tab?.let { tab ->
            tabInfo.value.collection?.let { collection ->

                CollectionItem(
                    tab = tab,
                    isLastInCollection = tabInfo.value.isLastInCollection,
                    onClick = { interactor.onCollectionOpenTabClicked(tab) },
                    onRemove = {
                        interactor.onCollectionRemoveTab(
                            collection = collection,
                            tab = tab,
                        )
                    },
                )
            }
        }
    }

    /**
     * Dynamically replace the current [Tab] shown in this `ViewHolder`.
     *
     * @param collection [TabCollection] containing [tab].
     * @param tab [Tab] to display.
     * @param isLastInCollection Whether [tab] is to be shown as the last item in [collection].
     */
    fun bindSession(collection: TabCollection, tab: Tab, isLastInCollection: Boolean) {
        tabData = TabInfo(collection, tab, isLastInCollection)
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

/**
 * Wrapper over a [Tab] adding information about the collection it is part of and the position in this collection.
 *
 * @property collection [TabCollection] which contains this tab.
 * @property tab [Tab] to display.
 * @property isLastInCollection Whether the tab is to be shown between others or as the last one in collection.
 */
@Stable
private data class TabInfo(
    val collection: TabCollection? = null,
    val tab: Tab? = null,
    val isLastInCollection: Boolean = false,
)
