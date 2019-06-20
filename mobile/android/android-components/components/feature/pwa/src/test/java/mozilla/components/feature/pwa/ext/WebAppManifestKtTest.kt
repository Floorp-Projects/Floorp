/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.graphics.Color.rgb
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.manifest.WebAppManifest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebAppManifestKtTest {

    @Test
    fun `should use name as label`() {
        val taskDescription = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com"
        ).asTaskDescription()
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
        ).asTaskDescription()
        assertEquals("My App", taskDescription.label)
        assertNull(taskDescription.icon)
        assertEquals(rgb(255, 0, 255), taskDescription.primaryColor)
    }
}
