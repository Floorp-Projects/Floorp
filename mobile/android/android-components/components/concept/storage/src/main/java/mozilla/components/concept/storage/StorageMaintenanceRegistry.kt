/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

/**
 * An interface which registers and unregisters storage maintenance WorkManager workers
 * that run maintenance on storages.
 */
interface StorageMaintenanceRegistry {

    /**
     * Registers a storage maintenance worker that prunes database when its size exceeds a size limit.
     * See also [Storage.runMaintenance].
     * */
    fun registerStorageMaintenanceWorker()

    /**
     * Unregisters the storage maintenance worker that is registered
     * by [StorageMaintenanceRegistry.registerStorageMaintenanceWorker].
     * See also [Storage.runMaintenance].
     *
     * @param uniqueWorkName Unique name of the work request that needs to be unregistered.
     * */
    fun unregisterStorageMaintenanceWorker(uniqueWorkName: String)
}
