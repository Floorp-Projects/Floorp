/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import androidx.lifecycle.ViewModel
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabCollection

class CreateCollectionViewModel : ViewModel() {
    var state = CollectionCreationState()
        private set

    var previousFragmentId: Int? = null

    fun updateCollection(
        tabs: List<Tab>,
        saveCollectionStep: SaveCollectionStep,
        selectedTabCollection: TabCollection,
        cachedTabCollections: List<TabCollection>
    ) {
        state = CollectionCreationState(
            tabs = tabs,
            selectedTabs = if (tabs.size == 1) setOf(tabs.first()) else emptySet(),
            tabCollections = cachedTabCollections.reversed(),
            selectedTabCollection = selectedTabCollection,
            saveCollectionStep = saveCollectionStep
        )
    }

    fun saveTabToCollection(
        tabs: List<Tab>,
        selectedTab: Tab?,
        cachedTabCollections: List<TabCollection>
    ) {
        val tabCollections = cachedTabCollections.reversed()
        state = CollectionCreationState(
            tabs = tabs,
            selectedTabs = selectedTab?.let { setOf(it) } ?: emptySet(),
            tabCollections = tabCollections,
            selectedTabCollection = null,
            saveCollectionStep = when {
                tabs.size > 1 -> SaveCollectionStep.SelectTabs
                tabCollections.isNotEmpty() -> SaveCollectionStep.SelectCollection
                else -> SaveCollectionStep.NameCollection
            }
        )
    }
}
