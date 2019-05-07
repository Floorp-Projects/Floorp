/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.feature.sync.BackgroundSyncManager

class TestBackgroundServices(
    context: Context,
    historyStorage: PlacesHistoryStorage,
    bookmarksStorage: PlacesBookmarksStorage
) : BackgroundServices(context, historyStorage, bookmarksStorage) {
    override val syncManager = BackgroundSyncManager("")
}
