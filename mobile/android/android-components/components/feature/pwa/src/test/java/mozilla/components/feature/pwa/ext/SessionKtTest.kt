/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SessionKtTest {
    private val demoManifest = WebAppManifest(name = "Demo", startUrl = "https://mozilla.com")
    private val demoIcon = WebAppManifest.Icon(src = "https://mozilla.com/example.png")

    @Test
    fun `web app must be HTTPS to be installable`() {
        val httpSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = false))
        }
        assertNull(httpSession.installableManifest())
    }

    @Test
    fun `web app must have manifest to be installable`() {
        val noManifestSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(null)
        }
        assertNull(noManifestSession.installableManifest())
    }

    @Test
    fun `web app must have an icon to be installable`() {
        val noIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(demoManifest)
        }
        assertNull(noIconSession.installableManifest())

        val noSizeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(demoIcon))
            )
        }
        assertNull(noSizeIconSession.installableManifest())

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
        assertNull(onlyBadgeIconSession.installableManifest())
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
        assertNull(smallIconSession.installableManifest())

        val weirdSizeSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(50, 200)))
                ))
            )
        }
        assertNull(weirdSizeSession.installableManifest())

        val largeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(192, 192)))
                ))
            )
        }
        assertEquals(
            demoManifest.copy(icons = listOf(
                demoIcon.copy(sizes = listOf(Size(192, 192)))
            )),
            largeIconSession.installableManifest()
        )

        val multiSizeIconSession = mock<Session>().also {
            whenever(it.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
            whenever(it.webAppManifest).thenReturn(
                demoManifest.copy(icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(16, 16), Size(512, 512)))
                ))
            )
        }
        assertEquals(
            demoManifest.copy(icons = listOf(
                demoIcon.copy(sizes = listOf(Size(16, 16), Size(512, 512)))
            )),
            multiSizeIconSession.installableManifest()
        )

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
        assertEquals(
            demoManifest.copy(icons = listOf(
                demoIcon.copy(sizes = listOf(Size(191, 193))),
                demoIcon.copy(sizes = listOf(Size(512, 512))),
                demoIcon.copy(
                    sizes = listOf(Size(192, 192)),
                    purpose = setOf(WebAppManifest.Icon.Purpose.BADGE)
                )
            )),
            multiIconSession.installableManifest()
        )
    }
}
