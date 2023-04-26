/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.SharedPreferences
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OnSharedPreferenceChangeListenerTest {

    private lateinit var sharedPrefs: SharedPreferences
    private lateinit var listener: (SharedPreferences, String) -> Unit
    private lateinit var owner: LifecycleOwner
    private lateinit var lifecycleRegistry: LifecycleRegistry

    @Before
    fun setup() {
        sharedPrefs = mockk(relaxUnitFun = true)
        listener = mockk(relaxed = true)
        owner = object : LifecycleOwner {
            override val lifecycle: Lifecycle
                get() = lifecycleRegistry
        }
        lifecycleRegistry = LifecycleRegistry(owner)
    }

    @Test
    fun `test listener is registered based on lifecycle`() {
        sharedPrefs.registerOnSharedPreferenceChangeListener(owner, listener)
        verify { sharedPrefs wasNot Called }

        lifecycleRegistry.currentState = Lifecycle.State.CREATED
        verify { sharedPrefs.registerOnSharedPreferenceChangeListener(any()) }

        lifecycleRegistry.currentState = Lifecycle.State.DESTROYED
        verify { sharedPrefs.unregisterOnSharedPreferenceChangeListener(any()) }
    }

    @Test
    fun `listener should call lambda`() {
        val wrapper = OnSharedPreferenceChangeListener(mockk(), listener)
        wrapper.onSharedPreferenceChanged(sharedPrefs, "key")

        verify { listener(sharedPrefs, "key") }
    }
}
