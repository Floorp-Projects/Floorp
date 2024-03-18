/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SessionStateKtTest {
    private val demoManifest = WebAppManifest(
        name = "Demo",
        startUrl = "https://mozilla.com",
        display = WebAppManifest.DisplayMode.STANDALONE,
    )
    private val demoIcon = WebAppManifest.Icon(src = "https://mozilla.com/example.png")

    @Test
    fun `web app must be HTTPS to be installable`() {
        val httpSession = createTestSession(secure = false)
        assertNull(httpSession.installableManifest())
    }

    @Test
    fun `web app must have manifest to be installable`() {
        val noManifestSession = createTestSession(
            secure = true,
            manifest = null,
        )
        assertNull(noManifestSession.installableManifest())
    }

    @Test
    fun `web app must have an icon to be installable`() {
        val noIconSession = createTestSession(
            secure = true,
            manifest = demoManifest,
        )
        assertNull(noIconSession.installableManifest())

        val noSizeIconSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(icons = listOf(demoIcon)),
        )
        assertNull(noSizeIconSession.installableManifest())

        val onlyBadgeIconSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(
                        sizes = listOf(Size(512, 512)),
                        purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME),
                    ),
                ),
            ),
        )
        assertNull(onlyBadgeIconSession.installableManifest())
    }

    @Test
    fun `web app must have 192x192 icons to be installable`() {
        val smallIconSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(32, 32))),
                ),
            ),
        )
        assertNull(smallIconSession.installableManifest())

        val weirdSizeSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(50, 200))),
                ),
            ),
        )
        assertNull(weirdSizeSession.installableManifest())

        val largeIconSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(192, 192))),
                ),
            ),
        )
        assertEquals(
            demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(192, 192))),
                ),
            ),
            largeIconSession.installableManifest(),
        )

        val multiSizeIconSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(16, 16), Size(512, 512))),
                ),
            ),
        )
        assertEquals(
            demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(16, 16), Size(512, 512))),
                ),
            ),
            multiSizeIconSession.installableManifest(),
        )

        val multiIconSession = createTestSession(
            secure = true,
            manifest = demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(191, 193))),
                    demoIcon.copy(sizes = listOf(Size(512, 512))),
                    demoIcon.copy(
                        sizes = listOf(Size(192, 192)),
                        purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME),
                    ),
                ),
            ),
        )
        assertEquals(
            demoManifest.copy(
                icons = listOf(
                    demoIcon.copy(sizes = listOf(Size(191, 193))),
                    demoIcon.copy(sizes = listOf(Size(512, 512))),
                    demoIcon.copy(
                        sizes = listOf(Size(192, 192)),
                        purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME),
                    ),
                ),
            ),
            multiIconSession.installableManifest(),
        )
    }
}

private fun createTestSession(
    secure: Boolean,
    manifest: WebAppManifest? = null,
): SessionState {
    val protocol = if (secure) {
        "https"
    } else {
        "http"
    }
    val tab = createTab("$protocol://www.mozilla.org")

    return tab.copy(
        content = tab.content.copy(
            securityInfo = SecurityInfoState(secure = secure),
            webAppManifest = manifest,
        ),
    )
}
