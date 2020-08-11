/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import mozilla.components.support.ktx.android.content.PreferencesHolder
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

fun featureFlagPreference(
    key: String,
    default: Boolean,
    featureFlag: Boolean
): ReadWriteProperty<PreferencesHolder, Boolean> =
    FeatureFlagPreferencePreference(key, default, featureFlag)

private class FeatureFlagPreferencePreference(
    private val key: String,
    private val default: Boolean,
    private val featureFlag: Boolean
) : ReadWriteProperty<PreferencesHolder, Boolean> {

    override fun getValue(thisRef: PreferencesHolder, property: KProperty<*>): Boolean =
        featureFlag && thisRef.preferences.getBoolean(key, default)

    override fun setValue(thisRef: PreferencesHolder, property: KProperty<*>, value: Boolean) =
        thisRef.preferences.edit().putBoolean(key, value).apply()
}
