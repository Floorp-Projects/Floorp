/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sharedpreferences

import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

private class BooleanPreference(
    private val key: String,
    private val default: Boolean
) : ReadWriteProperty<PreferencesHolder, Boolean> {

    override fun getValue(thisRef: PreferencesHolder, property: KProperty<*>): Boolean =
        thisRef.preferences.getBoolean(key, default)

    override fun setValue(thisRef: PreferencesHolder, property: KProperty<*>, value: Boolean) {
        thisRef.preferences.edit().putBoolean(key, value).apply()
    }
}

/**
 * Property delegate for getting and setting a boolean shared preference.
 */
fun booleanPreference(key: String, default: Boolean): ReadWriteProperty<PreferencesHolder, Boolean> =
    BooleanPreference(key, default)
