/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
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
    private lateinit var icons: BrowserIcons
    private lateinit var binding: QuicksettingsWebsiteInfoBinding

    @Before
    fun setup() {
        icons = mockk(relaxed = true)
        view = WebsiteInfoView(FrameLayout(testContext), icons)
        every { icons.loadIntoView(view.favicon_image, any()) } returns mockk()
        binding = view.binding
    }

    @Test
    fun bindUrlAndTitle() {
        val websiteUrl = "https://mozilla.org"

        view.update(WebsiteInfoState(
            websiteUrl = websiteUrl,
            websiteTitle = "Mozilla",
            websiteSecurityUiValues = WebsiteSecurityUiValues.SECURE,
            certificateName = ""
        ))

        verify { icons.loadIntoView(binding.favicon_image, IconRequest(websiteUrl)) }

        assertEquals("mozilla.org", binding.url.text)
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
