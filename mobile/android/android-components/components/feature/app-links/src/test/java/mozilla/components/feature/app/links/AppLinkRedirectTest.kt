/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.app.links

import android.content.Intent
import android.net.Uri
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`

class AppLinkRedirectTest {

    @Test
    fun hasExternalApp() {
        var appLink = AppLinkRedirect(appIntent = mock(), fallbackUrl = null, marketplaceIntent = null)
        assertTrue(appLink.hasExternalApp())
        assertTrue(appLink.isRedirect())

        appLink = AppLinkRedirect(appIntent = null, fallbackUrl = null, marketplaceIntent = null)
        assertFalse(appLink.hasExternalApp())
        assertFalse(appLink.isRedirect())
    }

    @Test
    fun hasFallback() {
        var appLink = AppLinkRedirect(appIntent = mock(), fallbackUrl = null, marketplaceIntent = null)
        assertFalse(appLink.hasFallback())
        assertTrue(appLink.isRedirect())

        appLink = AppLinkRedirect(appIntent = mock(), fallbackUrl = "https://example.com", marketplaceIntent = null)
        assertTrue(appLink.hasFallback())
        assertTrue(appLink.isRedirect())
    }

    @Test
    fun isRedirect() {
        var appLink = AppLinkRedirect(appIntent = null, fallbackUrl = null, marketplaceIntent = null)
        assertFalse(appLink.isRedirect())

        appLink = AppLinkRedirect(appIntent = mock(), fallbackUrl = null, marketplaceIntent = null)
        assertTrue(appLink.isRedirect())

        appLink = AppLinkRedirect(appIntent = null, fallbackUrl = "https://example.com", marketplaceIntent = null)
        assertTrue(appLink.isRedirect())

        appLink = AppLinkRedirect(appIntent = mock(), fallbackUrl = "https://example.com", marketplaceIntent = null)
        assertTrue(appLink.isRedirect())
    }

    @Test
    fun isInstallable() {
        val intent: Intent = mock()
        val uri: Uri = mock()
        `when`(intent.data).thenReturn(uri)
        `when`(uri.scheme).thenReturn("market")

        var appLink = AppLinkRedirect(appIntent = null, fallbackUrl = "https://example.com", marketplaceIntent = null)
        assertFalse(appLink.isInstallable())
        assertTrue(appLink.isRedirect())

        appLink = AppLinkRedirect(appIntent = intent, fallbackUrl = "https://example.com", marketplaceIntent = null)
        assertTrue(appLink.isInstallable())
        assertTrue(appLink.isRedirect())
    }

    @Test
    fun hasMarketplaceIntent() {
        var appLink = AppLinkRedirect(appIntent = null, fallbackUrl = null, marketplaceIntent = mock())
        assertTrue(appLink.hasMarketplaceIntent())
        assertTrue(appLink.isRedirect())
        assertTrue(appLink.hasMarketplaceIntent())
    }
}
