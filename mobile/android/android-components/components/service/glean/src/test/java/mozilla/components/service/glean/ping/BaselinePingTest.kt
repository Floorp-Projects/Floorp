/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.accessibilityservice.AccessibilityServiceInfo
import android.content.Context
import android.view.accessibility.AccessibilityManager
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows
import java.util.Locale

@RunWith(RobolectricTestRunner::class)
class BaselinePingTest {

    @Test
    fun `Make sure a11y services are collected`() {
        val applicationContext = ApplicationProvider.getApplicationContext<Context>()
        val accessibilityManager = applicationContext.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
        val baselinePing = BaselinePing(applicationContext)

        val shadowAccessibilityManager = Shadows.shadowOf(accessibilityManager)
        shadowAccessibilityManager.setEnabled(true)

        val serviceSpy1 = Mockito.spy<AccessibilityServiceInfo>(AccessibilityServiceInfo::class.java)
        Mockito.doReturn("service1").`when`(serviceSpy1).id

        val serviceSpy2 = Mockito.spy<AccessibilityServiceInfo>(AccessibilityServiceInfo::class.java)
        Mockito.doReturn("service2").`when`(serviceSpy2).id

        val nullServiceId = Mockito.spy<AccessibilityServiceInfo>(AccessibilityServiceInfo::class.java)
        Mockito.doReturn(null).`when`(nullServiceId).id

        shadowAccessibilityManager.setEnabledAccessibilityServiceList(
            listOf(serviceSpy1, serviceSpy2, nullServiceId)
        )

        assertEquals(
            listOf("service1", "service2"),
            baselinePing.getEnabledAccessibilityServices(
                accessibilityManager
            )
        )
    }

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
