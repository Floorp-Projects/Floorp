/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.SharedPreferences
import androidx.lifecycle.Lifecycle.Event.ON_CREATE
import androidx.lifecycle.Lifecycle.Event.ON_DESTROY
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent

class OnSharedPreferenceChangeListener(
    private val sharedPreferences: SharedPreferences,
    private val listener: (SharedPreferences, String) -> Unit
) : SharedPreferences.OnSharedPreferenceChangeListener, LifecycleObserver {

    @OnLifecycleEvent(ON_CREATE)
    fun onCreate() {
        sharedPreferences.registerOnSharedPreferenceChangeListener(this)
    }

    @OnLifecycleEvent(ON_DESTROY)
    fun onDestroy() {
        sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        listener(sharedPreferences, key)
    }
}

fun SharedPreferences.registerOnSharedPreferenceChangeListener(
    owner: LifecycleOwner,
    listener: (SharedPreferences, String) -> Unit
) {
    owner.lifecycle.addObserver(OnSharedPreferenceChangeListener(this, listener))
}
