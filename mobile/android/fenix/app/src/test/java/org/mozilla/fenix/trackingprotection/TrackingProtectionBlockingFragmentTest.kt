/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import androidx.fragment.app.FragmentActivity
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.trackingprotection.TrackingProtectionMode.CUSTOM
import org.robolectric.Robolectric

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionBlockingFragmentTest {
    @Test
    fun `GIVEN total cookie protection is enabled WHEN showing details about the protection options THEN show update details for tracking protection`() {
        val expectedTitle = testContext.getString(R.string.etp_cookies_title_2)
        val expectedDescription = testContext.getString(R.string.etp_cookies_description_2)

        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true) {
                every { enabledTotalCookieProtection } returns true
            }

            val fragment = createFragment()

            val cookiesCategory = fragment.binding.categoryCookies
            assertEquals(expectedTitle, cookiesCategory.trackingProtectionCategoryTitle.text)
            assertEquals(expectedDescription, cookiesCategory.trackingProtectionCategoryItemDescription.text)
        }
    }

    @Test
    fun `GIVEN total cookie protection is not enabled WHEN showing details about the protection options THEN show the default details for tracking protection`() {
        val expectedTitle = testContext.getString(R.string.etp_cookies_title)
        val expectedDescription = testContext.getString(R.string.etp_cookies_description)

        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true) {
                every { enabledTotalCookieProtection } returns false
            }

            val fragment = createFragment()

            val cookiesCategory = fragment.binding.categoryCookies
            assertEquals(expectedTitle, cookiesCategory.trackingProtectionCategoryTitle.text)
            assertEquals(expectedDescription, cookiesCategory.trackingProtectionCategoryItemDescription.text)
        }
    }

    private fun createFragment(): TrackingProtectionBlockingFragment {
        // Create and attach the fragment ourself instead of using "createAddedTestFragment"
        // to prevent having "onResume -> showToolbar" called.

        val activity = Robolectric.buildActivity(FragmentActivity::class.java)
            .create()
            .start()
            .get()
        val fragment = TrackingProtectionBlockingFragment().apply {
            arguments = TrackingProtectionBlockingFragmentArgs(
                protectionMode = CUSTOM,
            ).toBundle()
        }
        activity.supportFragmentManager.beginTransaction()
            .add(fragment, "test")
            .commitNow()

        return fragment
    }
}
