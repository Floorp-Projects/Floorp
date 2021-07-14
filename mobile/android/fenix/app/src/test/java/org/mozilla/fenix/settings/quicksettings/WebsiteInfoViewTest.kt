/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.QuicksettingsWebsiteInfoBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class WebsiteInfoViewTest {

    private lateinit var view: WebsiteInfoView
    private lateinit var icons: BrowserIcons
    private lateinit var binding: QuicksettingsWebsiteInfoBinding
    private lateinit var interactor: WebSiteInfoInteractor

    @Before
    fun setup() {
        icons = mockk(relaxed = true)
        interactor = mockk(relaxed = true)
        view = WebsiteInfoView(FrameLayout(testContext), icons, interactor)
        binding = view.binding
        every { icons.loadIntoView(view.favicon_image, any()) } returns mockk()
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

        verify { icons.loadIntoView(binding..favicon_image, IconRequest(websiteUrl)) }

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

    @Test
    fun `WHEN updating on detailed mode THEN bind the certificate, title and back button listener`() {
        val view = spyk(WebsiteInfoView(FrameLayout(testContext), icons, interactor, isDetailsMode = true))

        view.update(WebsiteInfoState(
            websiteUrl = "https://mozilla.org",
            websiteTitle = "Mozilla",
            websiteSecurityUiValues = WebsiteSecurityUiValues.INSECURE,
            certificateName = "Certificate"
        ))

        verify {
            view.bindCertificateName("Certificate")
            view.bindTitle("Mozilla")
            view.bindBackButtonListener()
        }
    }

    @Test
    fun `WHEN updating on not detailed mode THEN only connection details listener should be binded`() {
        val view = spyk(WebsiteInfoView(FrameLayout(testContext), icons, interactor, isDetailsMode = false))

        view.update(WebsiteInfoState(
            websiteUrl = "https://mozilla.org",
            websiteTitle = "Mozilla",
            websiteSecurityUiValues = WebsiteSecurityUiValues.INSECURE,
            certificateName = "Certificate"
        ))

        verify(exactly = 0) {
            view.bindCertificateName("Certificate")
            view.bindTitle("Mozilla")
            view.bindBackButtonListener()
        }

        verify {
            view.bindConnectionDetailsListener()
        }
    }

    @Test
    fun `WHEN rendering THEN use the correct layout`() {
        val normalView = WebsiteInfoView(FrameLayout(testContext), icons, interactor, isDetailsMode = false)

        assertEquals(R.layout.quicksettings_website_info, normalView.layoutId)

        val detailedView = WebsiteInfoView(FrameLayout(testContext), icons, interactor, isDetailsMode = true)

        assertEquals(R.layout.connection_details_website_info, detailedView.layoutId)
    }
}
