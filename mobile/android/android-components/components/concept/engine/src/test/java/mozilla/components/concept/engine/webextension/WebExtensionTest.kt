/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

class WebExtensionTest {

    @Test
    fun createWebExtension() {
        val ext1 = WebExtension("1", "url1")
        val ext2 = WebExtension("2", "url2")

        assertNotEquals(ext1, ext2)
        assertEquals(ext1, WebExtension("1", "url1"))
        assertEquals("1", ext1.id)
        assertEquals("url1", ext1.url)
    }
}