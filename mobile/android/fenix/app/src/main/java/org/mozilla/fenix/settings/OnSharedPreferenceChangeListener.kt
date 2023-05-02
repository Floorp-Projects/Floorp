/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.SharedPreferences
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner

class OnSharedPreferenceChangeListener(
    private val sharedPreferences: SharedPreferences,
    private val listener: (SharedPreferences, String?) -> Unit,
) : SharedPreferences.OnSharedPreferenceChangeListener, DefaultLifecycleObserver {

    override fun onCreate(owner: LifecycleOwner) {
        sharedPreferences.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onDestroy(owner: LifecycleOwner) {
        sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String?) {
        listener(sharedPreferences, key)
    }
}

/**
 * Registers a [OnSharedPreferenceChangeListener] as a [LifecycleObserver] for a preference.
 */
fun SharedPreferences.registerOnSharedPreferenceChangeListener(
    owner: LifecycleOwner,
    listener: (SharedPreferences, String?) -> Unit,
) {
    owner.lifecycle.addObserver(OnSharedPreferenceChangeListener(this, listener))
}
