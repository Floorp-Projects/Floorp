package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import androidx.lifecycle.ViewModel
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabCollection

class CreateCollectionViewModel : ViewModel() {
    var selectedTabs = setOf<Tab>()
    var tabs = listOf<Tab>()
    var saveCollectionStep: SaveCollectionStep = SaveCollectionStep.SelectTabs
    var tabCollections = listOf<TabCollection>()
}
