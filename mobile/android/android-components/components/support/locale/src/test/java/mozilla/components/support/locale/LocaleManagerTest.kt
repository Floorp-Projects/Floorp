/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class LocaleManagerTest {

    private lateinit var localeUseCases: LocaleUseCases

    @Before
    fun setup() {
        LocaleManager.clear(testContext)

        localeUseCases = mock()
        whenever(localeUseCases.notifyLocaleChanged).thenReturn(mock())
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `changing the language to Spanish must change the system locale to Spanish and change the configurations`() {
        var currentLocale = LocaleManager.getCurrentLocale(testContext)

        assertNull(currentLocale)

        val newContext = LocaleManager.setNewLocale(testContext, localeUseCases, "es".toLocale())

        assertNotEquals(testContext, newContext)

        currentLocale = LocaleManager.getCurrentLocale(testContext)

        assertEquals(currentLocale, "es".toLocale())
        assertEquals(currentLocale, Locale.getDefault())
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `when calling updateResources without a stored language we must not change the system locale`() {
        val previousSystemLocale = Locale.getDefault()
        LocaleManager.updateResources(testContext)

        assertEquals(previousSystemLocale, Locale.getDefault())
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `when resetting to system locale then the current locale will be the system one`() {
        assertEquals("en_US".toLocale(), Locale.getDefault())

        LocaleManager.setNewLocale(
            testContext,
            localeUseCases,
            "fr".toLocale(),
        )

        val storedLocale = Locale.getDefault()

        assertEquals("fr".toLocale(), Locale.getDefault())
        assertEquals("fr".toLocale(), storedLocale)

        LocaleManager.resetToSystemDefault(testContext, localeUseCases)

        assertEquals("en_US".toLocale(), Locale.getDefault())
        assertNull(LocaleManager.getCurrentLocale(testContext))
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `when setting a new locale then the current locale will be different than the system locale`() {
        assertEquals("en_US".toLocale(), Locale.getDefault())

        LocaleManager.setNewLocale(testContext, localeUseCases, "fr".toLocale())

        assertEquals("en_US".toLocale(), LocaleManager.getSystemDefault())
        assertEquals("fr".toLocale(), LocaleManager.getCurrentLocale(testContext))
        assertEquals("fr".toLocale(), Locale.getDefault())
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `when setting a new locale then the store is notified via use case`() {
        assertEquals("en_US".toLocale(), Locale.getDefault())

        LocaleManager.setNewLocale(testContext, localeUseCases, "fr".toLocale())

        verify(localeUseCases, times(1)).notifyLocaleChanged
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `GIVEN the locale use cases are defined WHEN resetting to system default locale THEN the store is notified via use case`() {
        assertEquals("en_US".toLocale(), Locale.getDefault())

        LocaleManager.setNewLocale(
            testContext,
            localeUseCases,
            "fr".toLocale(),
        )

        verify(localeUseCases, times(1)).notifyLocaleChanged

        val storedLocale = Locale.getDefault()

        assertEquals("fr".toLocale(), Locale.getDefault())
        assertEquals("fr".toLocale(), storedLocale)

        LocaleManager.resetToSystemDefault(testContext, localeUseCases)

        verify(localeUseCases, times(2)).notifyLocaleChanged
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `WHEN setting a new locale THEN locale use cases are used only when defined`() {
        assertEquals("en_US".toLocale(), Locale.getDefault())

        val locale1 = "fr".toLocale()
        LocaleManager.setNewLocale(
            testContext,
            locale = locale1,
        )

        verify(localeUseCases, never()).notifyLocaleChanged
        assertEquals(locale1, Locale.getDefault())

        val locale2 = "es".toLocale()
        LocaleManager.setNewLocale(
            testContext,
            localeUseCases,
            locale2,
        )

        verify(localeUseCases, times(1)).notifyLocaleChanged
        assertEquals(locale2, Locale.getDefault())
    }
}
