/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.library_site_item.title
import kotlinx.android.synthetic.main.library_site_item.url
import kotlinx.android.synthetic.main.quicksettings_website_info.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class WebsiteInfoViewTest {

    private lateinit var view: WebsiteInfoView

    @Before
    fun setup() {
        view = WebsiteInfoView(FrameLayout(testContext))
    }

    @Test
    fun bindUrlAndTitle() {
        view.update(WebsiteInfoState(
            websiteUrl = "https://mozilla.org",
            websiteTitle = "Mozilla",
            websiteSecurityUiValues = WebsiteSecurityUiValues.SECURE,
            certificateName = ""
        ))

        assertEquals("https://mozilla.org", view.url.text)
        assertEquals("Mozilla", view.title.text)
        assertEquals("Secure Connection", view.securityInfo.text)
        assertFalse(view.certificateInfo.isVisible)
    }

    @Test
    fun bindCert() {
        view.update(WebsiteInfoState(
            websiteUrl = "https://mozilla.org",
            websiteTitle = "Mozilla",
            websiteSecurityUiValues = WebsiteSecurityUiValues.INSECURE,
            certificateName = "Certificate"
        ))

        assertEquals("Insecure Connection", view.securityInfo.text)
        assertEquals("Verified By: Certificate", view.certificateInfo.text)
        assertTrue(view.certificateInfo.isVisible)
    }
}
