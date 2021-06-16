/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.QuicksettingsWebsiteInfoBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class WebsiteInfoViewTest {

    private lateinit var view: WebsiteInfoView
    private lateinit var binding: QuicksettingsWebsiteInfoBinding

    @Before
    fun setup() {
        view = WebsiteInfoView(FrameLayout(testContext))
        binding = view.binding
    }

    @Test
    fun bindUrlAndTitle() {
        view.update(
            WebsiteInfoState(
                websiteUrl = "https://mozilla.org",
                websiteTitle = "Mozilla",
                websiteSecurityUiValues = WebsiteSecurityUiValues.SECURE,
                certificateName = ""
            )
        )

        assertEquals("https://mozilla.org", binding.url.text)
        assertEquals("Secure Connection", binding.securityInfo.text)
    }

    @Test
    fun bindCert() {
        view.update(
            WebsiteInfoState(
                websiteUrl = "https://mozilla.org",
                websiteTitle = "Mozilla",
                websiteSecurityUiValues = WebsiteSecurityUiValues.INSECURE,
                certificateName = "Certificate"
            )
        )

        assertEquals("Insecure Connection", binding.securityInfo.text)
    }
}
