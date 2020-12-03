/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import org.junit.Assert.assertEquals
import org.junit.Test

class PaddingTest {

    @Test
    fun `padding should init its values`() {
        val padding = Padding(16, 24, 32, 40)

        val (start, top, end, bottom) = padding
        assertEquals(start, 16)
        assertEquals(top, 24)
        assertEquals(end, 32)
        assertEquals(bottom, 40)
    }
}
