/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.Locale

@RunWith(RobolectricTestRunner::class)
class BaselinePingTest {
    @Test
    fun `getLanguageTag() reports the tag for the default locale`() {
        val baselinePing = BaselinePing(ApplicationProvider.getApplicationContext<Context>())
        val defaultLanguageTag = baselinePing.getLanguageTag()

        assertNotNull(defaultLanguageTag)
        assertFalse(defaultLanguageTag.isEmpty())
        assertEquals("en-US", defaultLanguageTag)
    }

    @Test
    fun `getLanguageTag reports the correct tag for a non-default language`() {
        val baselinePing = BaselinePing(ApplicationProvider.getApplicationContext<Context>())
        val defaultLocale = Locale.getDefault()

        try {
            Locale.setDefault(Locale("fy", "NL"))

            val languageTag = baselinePing.getLanguageTag()

            assertNotNull(languageTag)
            assertFalse(languageTag.isEmpty())
            assertEquals("fy-NL", languageTag)
        } finally {
            Locale.setDefault(defaultLocale)
        }
    }

    @Test
    fun `getLanguage reports the modern translation for some languages`() {
        val baselinePing = BaselinePing(ApplicationProvider.getApplicationContext<Context>())

        assertEquals("he", baselinePing.getLanguage(Locale("iw", "IL")))
        assertEquals("id", baselinePing.getLanguage(Locale("in", "ID")))
        assertEquals("yi", baselinePing.getLanguage(Locale("ji", "ID")))
    }
}
