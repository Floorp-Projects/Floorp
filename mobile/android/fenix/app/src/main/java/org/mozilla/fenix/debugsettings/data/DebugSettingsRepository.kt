/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.data

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch

/**
 * [DataStore] for accessing debugging settings.
 */
private val Context.debugSettings: DataStore<Preferences> by preferencesDataStore(name = "debug_settings")

private val debugDrawerEnabledKey = booleanPreferencesKey("debug_drawer_enabled")

/**
 * Cache for accessing any settings related to debugging.
 */
interface DebugSettingsRepository {

    /**
     * [Flow] for checking whether the Debug Drawer is enabled.
     */
    val debugDrawerEnabled: Flow<Boolean>

    /**
     * Updates whether the debug drawer is enabled.
     *
     * @param enabled Whether the debug drawer is enabled.
     */
    fun setDebugDrawerEnabled(enabled: Boolean)
}

/**
 * The default implementation of [DebugSettingsRepository].
 *
 * @param context Android context used to obtain the underlying [DataStore].
 * @param dataStore [DataStore] for accessing debugging settings.
 * @param writeScope [CoroutineScope] used for writing settings changes to disk.
 */
class DefaultDebugSettingsRepository(
    context: Context,
    private val dataStore: DataStore<Preferences> = context.debugSettings,
    private val writeScope: CoroutineScope,
) : DebugSettingsRepository {

    override val debugDrawerEnabled: Flow<Boolean> =
        dataStore.data.map { preferences ->
            preferences[debugDrawerEnabledKey] ?: false
        }

    override fun setDebugDrawerEnabled(enabled: Boolean) {
        writeScope.launch {
            dataStore.edit { preferences ->
                preferences[debugDrawerEnabledKey] = enabled
            }
        }
    }
}
