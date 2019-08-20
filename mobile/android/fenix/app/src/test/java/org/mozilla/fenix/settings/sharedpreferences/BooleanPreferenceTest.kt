/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sharedpreferences

import android.content.SharedPreferences
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.verify
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class BooleanPreferenceTest {

    private lateinit var sharedPrefs: SharedPreferences
    private lateinit var editor: SharedPreferences.Editor

    @Before
    fun setup() {
        sharedPrefs = mockk(relaxed = true)
        editor = mockk()

        every { sharedPrefs.edit() } returns editor
        every { editor.putBoolean(any(), any()) } returns editor
        every { editor.apply() } just runs
    }

    @Test
    fun `getter returns boolean from shared preferences`() {
        val holder = object : PreferencesHolder {
            override val preferences = sharedPrefs
            val test by booleanPreference("test_pref_key", default = false)
        }
        every { sharedPrefs.getBoolean("test_pref_key", false) } returns true

        assertTrue(holder.test)
        verify { sharedPrefs.getBoolean("test_pref_key", false) }
    }

    @Test
    fun `setter applies boolean to shared preferences`() {
        val holder = object : PreferencesHolder {
            override val preferences = sharedPrefs
            var test by booleanPreference("pref", default = true)
        }
        holder.test = false

        verify { editor.putBoolean("pref", false) }
        verify { editor.apply() }
    }

    @Test
    fun `getter uses default value`() {
        val holderFalse = object : PreferencesHolder {
            override val preferences = sharedPrefs
            val test by booleanPreference("test_pref_key", default = false)
        }
        // Call the getter for the test
        holderFalse.test

        verify { sharedPrefs.getBoolean("test_pref_key", false) }

        val holderTrue = object : PreferencesHolder {
            override val preferences = sharedPrefs
            val test by booleanPreference("test_pref_key", default = true)
        }
        // Call the getter for the test
        holderTrue.test

        verify { sharedPrefs.getBoolean("test_pref_key", true) }
    }
}
