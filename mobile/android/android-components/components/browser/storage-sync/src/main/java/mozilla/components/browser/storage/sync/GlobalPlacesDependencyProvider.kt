/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.annotation.VisibleForTesting

/**
 * Provides global access to the dependencies needed for places storage operations.
 * */
object GlobalPlacesDependencyProvider {

    @VisibleForTesting
    internal var placesStorage: PlacesStorage? = null

    /**
     * Initializes places storage for running the maintenance task via [PlacesHistoryStorageWorker].
     * This method should be called in client application's onCreate method and before
     * [PlacesHistoryStorage.registerStorageMaintenanceWorker] in order to run the worker while
     * the app is not running.
     * */
    fun initialize(placesStorage: PlacesStorage) {
        this.placesStorage = placesStorage
    }

    /**
     * Provides [PlacesStorage] globally when needed for [PlacesHistoryStorageWorker]
     * to run maintenance on the storage.
     * */
    internal fun requirePlacesStorage(): PlacesStorage {
        return requireNotNull(placesStorage) {
            "GlobalPlacesDependencyProvider.initialize must be called before accessing the Places storage"
        }
    }
}
