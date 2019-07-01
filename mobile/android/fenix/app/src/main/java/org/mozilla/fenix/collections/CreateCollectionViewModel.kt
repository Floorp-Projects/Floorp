package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.View
import androidx.lifecycle.ViewModel
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabCollection

class CreateCollectionViewModel : ViewModel() {
    var selectedTabs = mutableSetOf<Tab>()
    var tabs = listOf<Tab>()
    var saveCollectionStep: SaveCollectionStep = SaveCollectionStep.SelectTabs
    var tabCollections = listOf<TabCollection>()
    var selectedTabCollection: TabCollection? = null
    var snackbarAnchorView: View? = null
    var previousFragmentId: Int? = null

    fun getStepForTabsAndCollectionSize(): SaveCollectionStep =
        if (tabs.size > 1) SaveCollectionStep.SelectTabs else tabCollections.getStepForCollectionsSize()
}

fun List<TabCollection>.getStepForCollectionsSize(): SaveCollectionStep =
    if (isEmpty()) SaveCollectionStep.NameCollection else SaveCollectionStep.SelectCollection

fun List<TabCollection>.getBackStepForCollectionsSize(): SaveCollectionStep =
    if (isEmpty()) SaveCollectionStep.SelectTabs else SaveCollectionStep.SelectCollection
