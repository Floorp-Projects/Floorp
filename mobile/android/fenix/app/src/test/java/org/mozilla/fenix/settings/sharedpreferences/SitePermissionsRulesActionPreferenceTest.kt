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
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class SitePermissionsRulesActionPreferenceTest {

    private lateinit var sharedPrefs: SharedPreferences
    private lateinit var editor: SharedPreferences.Editor

    @Before
    fun setup() {
        sharedPrefs = mockk(relaxed = true)
        editor = mockk()

        every { sharedPrefs.edit() } returns editor
        every { editor.putInt(any(), any()) } returns editor
        every { editor.apply() } just runs
    }

    @Test
    fun `getter returns action from shared preferences`() {
        val holder = object : PreferencesHolder {
            override val preferences = sharedPrefs
            val test by sitePermissionsRulesActionPreference("test_preference_key")
        }
        every { sharedPrefs.getInt("test_preference_key", 1) } returns 0

        assertEquals(SitePermissionsRules.Action.BLOCKED, holder.test)
        verify { sharedPrefs.getInt("test_preference_key", 1) }
    }

    @Test
    fun `setter applies boolean to shared preferences`() {
        val holder = object : PreferencesHolder {
            override val preferences = sharedPrefs
            var test by sitePermissionsRulesActionPreference("pref")
        }
        holder.test = SitePermissionsRules.Action.BLOCKED

        verify { editor.putInt("pref", 0) }
        verify { editor.apply() }

        holder.test = SitePermissionsRules.Action.ASK_TO_ALLOW

        verify { editor.putInt("pref", 1) }
        verify { editor.apply() }
    }

    @Test
    fun `getter defaults to ASK_TO_ALLOW`() {
        every { sharedPrefs.getInt("key", 1) } returns 1
        val holder = object : PreferencesHolder {
            override val preferences = sharedPrefs
            val action by sitePermissionsRulesActionPreference("key")
        }

        assertEquals(SitePermissionsRules.Action.ASK_TO_ALLOW, holder.action)
        verify { sharedPrefs.getInt("key", 1) }
    }
}
