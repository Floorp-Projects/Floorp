/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import androidx.preference.Preference
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class CustomEtpCookiesOptionsDropDownListPreferenceTest {
    @Test
    fun `GIVEN total cookie protection is enabled WHEN using this preference THEN show the total cookie protection option`() {
        val expectedEntries = arrayOf(
            testContext.getString(R.string.preference_enhanced_tracking_protection_custom_cookies_5),
        ) + defaultEntries
        val expectedValues = arrayOf(testContext.getString(R.string.total_protection)) + defaultValues

        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk {
                every { enabledTotalCookieProtection } returns true
            }

            val preference = CustomEtpCookiesOptionsDropDownListPreference(testContext)

            assertArrayEquals(expectedEntries, preference.entries)
            assertArrayEquals(expectedValues, preference.entryValues)
            assertEquals(expectedValues[0], preference.getDefaultValue())
        }
    }

    @Test
    fun `GIVEN total cookie protection is disabled WHEN using this preference THEN don't show the total cookie protection option`() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk {
                every { enabledTotalCookieProtection } returns false
            }

            val preference = CustomEtpCookiesOptionsDropDownListPreference(testContext)

            assertArrayEquals(defaultEntries, preference.entries)
            assertArrayEquals(defaultValues, preference.entryValues)
            assertEquals(defaultValues[0], preference.getDefaultValue())
        }
    }

    /**
     * Use reflection to get the private member holding the default value set for this preference.
     */
    private fun CustomEtpCookiesOptionsDropDownListPreference.getDefaultValue(): String {
        return Preference::class.java
            .getDeclaredField("mDefaultValue").let { field ->
                field.isAccessible = true
                return@let field.get(this) as String
            }
    }

    private val defaultEntries = with(testContext) {
        arrayOf(
            getString(R.string.preference_enhanced_tracking_protection_custom_cookies_1),
            getString(R.string.preference_enhanced_tracking_protection_custom_cookies_2),
            getString(R.string.preference_enhanced_tracking_protection_custom_cookies_3),
            getString(R.string.preference_enhanced_tracking_protection_custom_cookies_4),
        )
    }

    private val defaultValues = with(testContext) {
        arrayOf(
            getString(R.string.social),
            getString(R.string.unvisited),
            getString(R.string.third_party),
            getString(R.string.all),
        )
    }
}
