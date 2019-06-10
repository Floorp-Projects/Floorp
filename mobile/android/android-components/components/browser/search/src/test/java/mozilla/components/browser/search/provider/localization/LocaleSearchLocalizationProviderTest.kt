/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider.localization

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class LocaleSearchLocalizationProviderTest {
    private var cachedLocale: Locale? = null

    @Before
    fun rememberLocale() {
        // Remember locale to restore the default after running the tests. Otherwise we might break
        // other tests that do not expect the locale to change.
        cachedLocale = Locale.getDefault()
    }

    @After
    fun restoreLocale() {
        cachedLocale?.let { Locale.setDefault(it) }
    }

    @Test
    fun `default in test is en-US`() {
        val provider = LocaleSearchLocalizationProvider()

        assertEquals("en", provider.language)
        assertEquals("US", provider.country)
        assertNull(provider.region)
    }

    @Test
    fun `provider returns new data if default locale changes`() {
        Locale.setDefault(Locale("de", "DE"))

        val provider = LocaleSearchLocalizationProvider()

        assertEquals("de", provider.language)
        assertEquals("DE", provider.country)
        assertNull(provider.region)
    }
}
