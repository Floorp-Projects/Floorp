/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

/**
 * An interface which provides generic operations for storing browser data like history and bookmarks.
 */
interface Storage {

    /**
     * Runs internal database maintenance tasks
     */
    suspend fun runMaintenance()

    /**
     * Cleans up background work and database connections
     */
    fun cleanup()
}
