/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SizeTest {
    @Test
    fun `parse standard sizes`() {
        assertEquals(Size(512, 512), Size.parse("512x512"))
        assertEquals(Size(16, 16), Size.parse("16x16"))
        assertEquals(Size(100, 250), Size.parse("100x250"))
    }

    @Test
    fun `parse any size`() {
        assertEquals(Size.ANY, Size.parse("any"))
    }

    @Test
    fun `return null for invalid sizes`() {
        assertNull(Size.parse("192"))
        assertNull(Size.parse("anywhere"))
        assertNull(Size.parse("fooxbar"))
        assertNull(Size.parse("x256"))
        assertNull(Size.parse("64x"))
    }
}
