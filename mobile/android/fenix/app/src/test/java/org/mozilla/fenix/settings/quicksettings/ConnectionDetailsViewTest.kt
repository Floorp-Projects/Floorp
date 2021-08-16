/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.ConnectionDetailsWebsiteInfoBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ConnectionDetailsViewTest {

    private lateinit var view: ConnectionDetailsView
    private lateinit var icons: BrowserIcons
    private lateinit var binding: ConnectionDetailsWebsiteInfoBinding
    private lateinit var interactor: WebSiteInfoInteractor

    @Before
    fun setup() {
        icons = mockk(relaxed = true)
        interactor = mockk(relaxed = true)
        view = spyk(ConnectionDetailsView(FrameLayout(testContext), icons, interactor))
        binding = view.binding
        every { icons.loadIntoView(any(), any()) } returns mockk()
    }

    @Test
    fun `WHEN updating THEN bind url and title`() {
        val websiteUrl = "https://mozilla.org"

        view.update(
            WebsiteInfoState(
                websiteUrl = websiteUrl,
                websiteTitle = "Mozilla",
                websiteSecurityUiValues = WebsiteSecurityUiValues.SECURE,
                certificateName = ""
            )
        )

        verify { icons.loadIntoView(binding.faviconImage, IconRequest(websiteUrl)) }

        assertEquals("https://mozilla.org", binding.url.text)
        assertEquals("Secure Connection", binding.securityInfo.text)
    }

    @Test
    fun `WHEN updating THEN bind certificate`() {
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
    fun `WHEN updating THEN bind the certificate, title and back button listener`() {
        view.update(
            WebsiteInfoState(
                websiteUrl = "https://mozilla.org",
                websiteTitle = "Mozilla",
                websiteSecurityUiValues = WebsiteSecurityUiValues.INSECURE,
                certificateName = "Certificate"
            )
        )

        verify {
            view.bindCertificateName("Certificate")
            view.bindTitle("Mozilla")
            view.bindBackButtonListener()
        }
    }

    @Test
    fun `WHEN title is empty THEN the title should be gone`() {

        view.bindTitle("")

        assertFalse(binding.titleContainer.isVisible)

        view.bindTitle("Title")

        assertTrue(binding.titleContainer.isVisible)
    }
}
