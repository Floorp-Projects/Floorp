/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.settings

import android.content.SharedPreferences
import io.mockk.Called
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.verify
import mozilla.components.support.ktx.android.content.PreferencesHolder
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class FeatureFlagPreferenceTest {

    @MockK private lateinit var prefs: SharedPreferences
    @MockK private lateinit var editor: SharedPreferences.Editor

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { prefs.getBoolean("key", false) } returns true
        every { prefs.edit() } returns editor
        every { editor.putBoolean("key", any()) } returns editor
        every { editor.apply() } just Runs
    }

    @Test
    fun `acts like boolean preference if feature flag is true`() {
        val holder = FeatureFlagHolder(featureFlag = true)

        assertTrue(holder.property)
        verify { prefs.getBoolean("key", false) }

        holder.property = false
        verify { editor.putBoolean("key", false) }
    }

    @Test
    fun `no-op if feature flag is false`() {
        val holder = FeatureFlagHolder(featureFlag = false)

        assertFalse(holder.property)
        holder.property = true
        holder.property = false

        verify { prefs wasNot Called }
        verify { editor wasNot Called }
    }

    private inner class FeatureFlagHolder(featureFlag: Boolean) : PreferencesHolder {
        override val preferences = prefs

        var property by featureFlagPreference(
            "key",
            default = false,
            featureFlag = featureFlag
        )
    }
}
