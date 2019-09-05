/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.location.search

import kotlinx.coroutines.runBlocking
import mozilla.components.service.location.MozillaLocationService
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import java.util.Locale

class RegionSearchLocalizationProviderTest {
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
    fun `Uses region from service`() {
        runBlocking {
            val service: MozillaLocationService = mock()
            doReturn(MozillaLocationService.Region("RU", "Russia"))
                .`when`(service).fetchRegion(ArgumentMatchers.anyBoolean())

            val provider = RegionSearchLocalizationProvider(service)
            val localization = provider.determineRegion()

            assertEquals("RU", localization.region)
            assertEquals("en", localization.language)
            assertEquals("US", localization.country)
        }
    }

    @Test
    fun `Uses country and language from locale`() {
        runBlocking {
            Locale.setDefault(Locale("de", "DE"))

            val service: MozillaLocationService = mock()
            doReturn(MozillaLocationService.Region("RU", "Russia"))
                .`when`(service).fetchRegion(ArgumentMatchers.anyBoolean())

            val provider = RegionSearchLocalizationProvider(service)
            val localization = provider.determineRegion()

            assertEquals("de", localization.language)
            assertEquals("DE", localization.country)
            assertEquals("RU", localization.region)
        }
    }

    @Test
    fun `Uses locale country if service does not return region`() {
        runBlocking {
            Locale.setDefault(Locale("de", "DE"))

            val service: MozillaLocationService = mock()
            doReturn(null)
                .`when`(service).fetchRegion(ArgumentMatchers.anyBoolean())

            val provider = RegionSearchLocalizationProvider(service)
            val localization = provider.determineRegion()

            assertEquals("de", localization.language)
            assertEquals("DE", localization.country)
            assertEquals("DE", localization.region)
        }
    }
}
