/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.content.SharedPreferences.Editor
import android.widget.CompoundButton.OnCheckedChangeListener
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.robolectric.Robolectric
import kotlin.random.Random

@RunWith(FenixRobolectricTestRunner::class)
class PreferenceBackedRadioButtonTest {
    @Test
    fun `GIVEN a preference key is provided WHEN initialized THEN cache the value`() {
        val attributes = Robolectric.buildAttributeSet()
            .addAttribute(R.attr.preferenceKey, "test")
            .build()
        every { testContext.settings().preferences.getBoolean(any(), any()) } returns Random.nextBoolean()

        val button = PreferenceBackedRadioButton(testContext, attributes)

        assertEquals("test", button.backingPreferenceName)
    }

    @Test
    fun `GIVEN a default preference value is provided WHEN initialized THEN cache the value`() {
        val attributes = Robolectric.buildAttributeSet()
            .addAttribute(R.attr.preferenceKeyDefaultValue, "true")
            .build()
        every { testContext.settings().preferences.getBoolean(any(), any()) } returns Random.nextBoolean()

        val button = PreferenceBackedRadioButton(testContext, attributes)

        assertTrue(button.backingPreferenceDefaultValue)
    }

    @Test
    fun `GIVEN a default preference value is not provided WHEN initialized THEN remember the default value as false`() {
        val attributes = Robolectric.buildAttributeSet().build()
        every { testContext.settings().preferences.getBoolean(any(), any()) } returns Random.nextBoolean()

        val button = PreferenceBackedRadioButton(testContext, attributes)

        assertFalse(button.backingPreferenceDefaultValue)
    }

    @Test
    fun `GIVEN the backing preference doesn't have a value set WHEN initialized THEN set if checked the default value`() {
        val attributes = Robolectric.buildAttributeSet()
            .addAttribute(R.attr.preferenceKeyDefaultValue, "true")
            .build()

        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns Settings(testContext)

            val button = PreferenceBackedRadioButton(testContext, attributes)

            assertTrue(button.isChecked)
        }
    }

    @Test
    fun `GIVEN there is no backing preference or default value set vaWHEN initialized THEN set if checked as false`() {
        val attributes = Robolectric.buildAttributeSet().build()

        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns Settings(testContext)

            val button = PreferenceBackedRadioButton(testContext, attributes)

            assertFalse(button.isChecked)
        }
    }

    @Test
    fun `GIVEN the backing preference does have a value set WHEN initialized THEN set if checked the value from the preference`() {
        val attributes = Robolectric.buildAttributeSet()
            .addAttribute(R.attr.preferenceKey, "test")
            .build()
        every { testContext.settings().preferences.getBoolean(eq("test"), any()) } returns true

        val button = PreferenceBackedRadioButton(testContext, attributes)

        assertTrue(button.isChecked)
    }

    @Test
    fun `WHEN a OnCheckedChangeListener is set THEN cache it internally`() {
        every { testContext.settings().preferences.getBoolean(any(), any()) } returns Random.nextBoolean()
        val button = PreferenceBackedRadioButton(testContext)
        val testListener: OnCheckedChangeListener = mockk()

        button.setOnCheckedChangeListener(testListener)

        assertSame(testListener, button.externalOnCheckedChangeListener)
    }

    @Test
    fun `GIVEN a OnCheckedChangeListener is set WHEN the checked status changes THEN update the backing preference and then inform the listener`() {
        val editor: Editor = mockk(relaxed = true)
        every { testContext.settings().preferences.edit() } returns editor
        // set the button initially as not checked
        every { testContext.settings().preferences.getBoolean(any(), any()) } returns false
        val button = PreferenceBackedRadioButton(testContext)
        button.backingPreferenceName = "test"
        val testListener: OnCheckedChangeListener = mockk(relaxed = true)
        button.externalOnCheckedChangeListener = testListener

        button.isChecked = true

        verifyOrder {
            editor.putBoolean("test", true)
            testListener.onCheckedChanged(any(), any())
        }
    }

    @Test
    fun `WHEN the button gets enabled THEN set isChecked based on the value from the backing preference`() {
        every { testContext.settings().preferences.getBoolean(any(), any()) } returns true
        val button = spyk(PreferenceBackedRadioButton(testContext))
        every { button.isChecked = any() } returns Unit

        button.isEnabled = true

        verify(exactly = 1) { // first "isChecked" from init happens before we can count it
            button.isChecked = true
        }
    }
}
