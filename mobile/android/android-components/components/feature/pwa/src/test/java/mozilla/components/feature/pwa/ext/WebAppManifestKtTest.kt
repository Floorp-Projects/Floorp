/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.graphics.Color
import android.graphics.Color.rgb
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebAppManifestKtTest {

    private val demoManifest = WebAppManifest(name = "Demo", startUrl = "https://mozilla.com")
    private val demoIcon = WebAppManifest.Icon(src = "https://mozilla.com/example.png")

    @Test
    fun `should use name as label`() {
        val taskDescription = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com",
        ).toTaskDescription(null)
        assertEquals("Demo", taskDescription.label)
        assertEquals(0, taskDescription.primaryColor)
    }

    @Test
    fun `should use themeColor as primaryColor`() {
        val taskDescription = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com",
            themeColor = rgb(255, 0, 255),
        ).toTaskDescription(null)
        assertEquals("My App", taskDescription.label)
        assertEquals(rgb(255, 0, 255), taskDescription.primaryColor)
    }

    @Test
    fun `should use themeColor as toolbarColor`() {
        val config = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com",
            themeColor = rgb(255, 0, 255),
            backgroundColor = rgb(230, 230, 230),
        ).toCustomTabConfig()
        assertEquals(rgb(255, 0, 255), config.toolbarColor)
        assertEquals(Color.WHITE, config.navigationBarColor)
        assertNull(config.closeButtonIcon)
        assertTrue(config.enableUrlbarHiding)
        assertNull(config.actionButtonConfig)
        assertTrue(config.showShareMenuItem)
        assertEquals(0, config.menuItems.size)
    }

    @Test
    fun `should return the scope as a uri`() {
        val scope = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com/pwa",
            scope = "https://example.com/",
            display = WebAppManifest.DisplayMode.STANDALONE,
        ).getTrustedScope()
        assertEquals("https://example.com/".toUri(), scope)

        val fallbackToStartUrl = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com/pwa",
            display = WebAppManifest.DisplayMode.STANDALONE,
        ).getTrustedScope()
        assertEquals("https://example.com/pwa".toUri(), fallbackToStartUrl)
    }

    @Test
    fun `should not return the scope if display mode is minimal-ui`() {
        val scope = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com/pwa",
            scope = "https://example.com/",
            display = WebAppManifest.DisplayMode.MINIMAL_UI,
        ).getTrustedScope()
        assertNull(scope)

        val fallbackToStartUrl = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com/pwa",
            display = WebAppManifest.DisplayMode.MINIMAL_UI,
        ).getTrustedScope()
        assertNull(fallbackToStartUrl)
    }

    @Test
    fun `web app must have an icon to be installable`() {
        val noIconManifest = demoManifest
        assertFalse(noIconManifest.hasLargeIcons())

        val noSizeIconManifest = demoManifest.copy(icons = listOf(demoIcon))
        assertFalse(noSizeIconManifest.hasLargeIcons())

        val onlyBadgeIconManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(
                    sizes = listOf(Size(512, 512)),
                    purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME),
                ),
            ),
        )
        assertFalse(onlyBadgeIconManifest.hasLargeIcons())
    }

    @Test
    fun `web app must have 192x192 icons to be installable`() {
        val smallIconManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(sizes = listOf(Size(32, 32))),
            ),
        )
        assertFalse(smallIconManifest.hasLargeIcons())

        val weirdSizeManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(sizes = listOf(Size(50, 200))),
            ),
        )
        assertFalse(weirdSizeManifest.hasLargeIcons())

        val largeIconManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(sizes = listOf(Size(192, 192))),
            ),
        )
        assertTrue(largeIconManifest.hasLargeIcons())

        val multiSizeIconManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(sizes = listOf(Size(16, 16), Size(512, 512))),
            ),
        )
        assertTrue(multiSizeIconManifest.hasLargeIcons())

        val multiIconManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(sizes = listOf(Size(191, 193))),
                demoIcon.copy(sizes = listOf(Size(512, 512))),
                demoIcon.copy(
                    sizes = listOf(Size(192, 192)),
                    purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME),
                ),
            ),
        )
        assertTrue(multiIconManifest.hasLargeIcons())

        val onlyBadgeManifest = demoManifest.copy(
            icons = listOf(
                demoIcon.copy(sizes = listOf(Size(191, 191))),
                demoIcon.copy(
                    sizes = listOf(Size(192, 192)),
                    purpose = setOf(WebAppManifest.Icon.Purpose.MONOCHROME),
                ),
            ),
        )
        assertFalse(onlyBadgeManifest.hasLargeIcons())
    }
}
