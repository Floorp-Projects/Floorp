/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionKtTest {
    private val demoManifest = WebAppManifest(name = "Demo", startUrl = "https://mozilla.com")
    private val demoIcon = WebAppManifest.Icon(src = "https://mozilla.com/example.png")

    @Test
    fun `web app must be HTTPS to be installable`() {
        val httpSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = false))
        }
        assertFalse(httpSession.isInstallable())
    }

    @Test
    fun `web app must have manifest to be installable`() {
        val noManifestSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(null)
        }
        assertFalse(noManifestSession.isInstallable())
    }

    @Test
    fun `web app must have an icon to be installable`() {
        val noIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(demoManifest)
        }
        assertFalse(noIconSession.isInstallable())

        val noSizeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(demoIcon))
            )
        }
        assertFalse(noSizeIconSession.isInstallable())

        val onlyBadgeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(
                        sizes = listOf(Size(512, 512)),
                        purpose = setOf(WebAppManifest.Icon.Purpose.BADGE)
                    )
                ))
            )
        }
        assertFalse(onlyBadgeIconSession.isInstallable())
    }

    @Test
    fun `web app must have 192x192 icons to be installable`() {
        val smallIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(32, 32)))
                ))
            )
        }
        assertFalse(smallIconSession.isInstallable())

        val weirdSizeSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(50, 200)))
                ))
            )
        }
        assertFalse(weirdSizeSession.isInstallable())

        val largeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(192, 192)))
                ))
            )
        }
        assertTrue(largeIconSession.isInstallable())

        val multiSizeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(16, 16), Size(512, 512)))
                ))
            )
        }
        assertTrue(multiSizeIconSession.isInstallable())

        val multiIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(191, 193))),
                    demoIcon.copy(sizes = listOf(Size(512, 512))),
                    demoIcon.copy(
                        sizes = listOf(Size(192, 192)),
                        purpose = setOf(WebAppManifest.Icon.Purpose.BADGE)
                    )
                ))
            )
        }
        assertTrue(multiIconSession.isInstallable())
    }
}
