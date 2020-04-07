/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleManager.getSystemDefault
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import java.lang.String.format
import java.util.Locale

class ContextTest {
    private lateinit var context: Context
    private val selectedLocale = Locale("ro", "RO")
    private val appName = "Firefox Preview"
    private val correctlyFormattedString = "Incearca noul %1s"
    private val incorrectlyFormattedString = "Incearca noul %1&amp;s"
    private val englishString = "Try the new %1s"

    private val mockId: Int = 11

    @Before
    fun setup() {
        mockkStatic("org.mozilla.fenix.settings.advanced.LocaleManagerExtensionKt")
        mockkObject(LocaleManager)
        context = mockk(relaxed = true)
        context.resources.configuration.setLocale(selectedLocale)

        every { LocaleManager.getCurrentLocale(context) } returns selectedLocale
    }

    @Test
    fun `getStringWithArgSafe returns selected locale for correct formatted string`() {
        every { context.getString(mockId) } returns correctlyFormattedString

        val result = context.getStringWithArgSafe(mockId, appName)

        assertEquals("Incearca noul Firefox Preview", result)
    }

    @Test
    fun `getStringWithArgSafe returns English locale for incorrect formatted string`() {

        every { getSystemDefault() } returns Locale("en")
        every { context.getString(mockId) } returns incorrectlyFormattedString
        every { format(context.getString(mockId), appName) } returns format(englishString, appName)

        val result = context.getStringWithArgSafe(mockId, appName)

        assertEquals("Try the new Firefox Preview", result)
    }
}
