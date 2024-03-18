/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import android.content.Context
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.sync.toSyncEngine

/**
 * Storage layer for the enabled/disabled state of [SyncEngine].
 */
class SyncEnginesStorage(private val context: Context) {
    companion object {
        const val SYNC_ENGINES_KEY = "syncEngines"
    }

    /**
     * @return A map describing known enabled/disabled state of [SyncEngine].
     */
    fun getStatus(): Map<SyncEngine, Boolean> {
        val resultMap = mutableMapOf<SyncEngine, Boolean>()
        // When adding new SyncEngines, think through implications for default values.
        // See https://github.com/mozilla-mobile/android-components/issues/4557

        // TODO does this need to be reversed? Go through what we have in local storage, and populate
        // result map based on that. reason: we may have "other" engines.
        // this will be empty if `setStatus` was never called.
        storage().all.forEach {
            if (it.value is Boolean) {
                resultMap[it.key.toSyncEngine()] = it.value as Boolean
            }
        }

        return resultMap
    }

    /**
     * Update enabled/disabled state of [engine].
     *
     * @param engine A [SyncEngine] for which to update state.
     * @param status New state.
     */
    fun setStatus(engine: SyncEngine, status: Boolean) {
        storage().edit().putBoolean(engine.nativeName, status).apply()
    }

    /**
     * Clears out any stored [SyncEngine] state.
     */
    internal fun clear() {
        storage().edit().clear().apply()
    }

    private fun storage() = context.getSharedPreferences(SYNC_ENGINES_KEY, Context.MODE_PRIVATE)
}
