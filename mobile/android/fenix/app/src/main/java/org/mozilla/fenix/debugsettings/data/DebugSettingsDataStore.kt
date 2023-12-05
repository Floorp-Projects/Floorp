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
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

/**
 * [DataStore] for accessing debugging settings.
 */
val Context.debugSettings: DataStore<Preferences> by preferencesDataStore(name = "debug_settings")

private val DEBUG_DRAWER_ENABLED = booleanPreferencesKey("debug_drawer_enabled")

/**
 * Updates whether the debug drawer is enabled.
 *
 * @param enabled Whether the debug drawer is enabled.
 */
suspend fun DataStore<Preferences>.updateDebugDrawerEnabled(enabled: Boolean) {
    edit { preferences ->
        preferences[DEBUG_DRAWER_ENABLED] = enabled
    }
}

/**
 * [Flow] for checking whether the Debug Drawer is enabled.
 */
val DataStore<Preferences>.debugDrawerEnabled: Flow<Boolean>
    get() = data.map { preferences ->
        preferences[DEBUG_DRAWER_ENABLED] ?: false
    }
