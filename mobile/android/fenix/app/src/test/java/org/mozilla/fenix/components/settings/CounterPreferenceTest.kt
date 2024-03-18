/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.settings

import android.content.SharedPreferences
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.verify
import mozilla.components.support.ktx.android.content.PreferencesHolder
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class CounterPreferenceTest {

    @MockK private lateinit var prefs: SharedPreferences

    @MockK private lateinit var editor: SharedPreferences.Editor

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { prefs.getInt("key", 0) } returns 0
        every { prefs.edit() } returns editor
        every { editor.putInt("key", any()) } returns editor
        every { editor.apply() } just Runs
    }

    @Test
    fun `update value after increment`() {
        val holder = CounterHolder()

        assertEquals(0, holder.property.value)
        holder.property.increment()

        verify { editor.putInt("key", 1) }
    }

    @Test
    fun `check if value is under max count`() {
        val holder = CounterHolder(maxCount = 2)

        every { prefs.getInt("key", 0) } returns 0
        assertEquals(0, holder.property.value)
        assertTrue(holder.property.underMaxCount())

        every { prefs.getInt("key", 0) } returns 2
        assertEquals(2, holder.property.value)
        assertFalse(holder.property.underMaxCount())
    }

    private inner class CounterHolder(maxCount: Int = -1) : PreferencesHolder {
        override val preferences = prefs

        val property = counterPreference("key", maxCount)
    }
}
