/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.graphics.Color
import android.graphics.Color.rgb
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.manifest.WebAppManifest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebAppManifestKtTest {

    @Test
    fun `should use name as label`() {
        val taskDescription = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com"
        ).toTaskDescription(null)
        assertEquals("Demo", taskDescription.label)
        assertNull(taskDescription.icon)
        assertEquals(0, taskDescription.primaryColor)
    }

    @Test
    fun `should use themeColor as primaryColor`() {
        val taskDescription = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com",
            themeColor = rgb(255, 0, 255)
        ).toTaskDescription(null)
        assertEquals("My App", taskDescription.label)
        assertNull(taskDescription.icon)
        assertEquals(rgb(255, 0, 255), taskDescription.primaryColor)
    }

    @Test
    fun `should use themeColor as toolbarColor`() {
        val config = WebAppManifest(
            name = "My App",
            startUrl = "https://example.com",
            themeColor = rgb(255, 0, 255),
            backgroundColor = rgb(230, 230, 230)
        ).toCustomTabConfig()
        assertEquals("https://example.com", config.id)
        assertEquals(rgb(255, 0, 255), config.toolbarColor)
        assertEquals(Color.WHITE, config.navigationBarColor)
        assertNull(config.closeButtonIcon)
        assertTrue(config.enableUrlbarHiding)
        assertNull(config.actionButtonConfig)
        assertTrue(config.showShareMenuItem)
        assertEquals(0, config.menuItems.size)
    }
}
