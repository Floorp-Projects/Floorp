/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.datastore

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.dataStore

/**
 * Application / process unique [DataStore] for IO operations related to Pocket recommended stories selected categories.
 */
internal val Context.pocketStoriesSelectedCategoriesDataStore: DataStore<SelectedPocketStoriesCategories> by dataStore(
    fileName = "pocket_recommendations_selected_categories.pb",
    serializer = SelectedPocketStoriesCategorySerializer,
)
