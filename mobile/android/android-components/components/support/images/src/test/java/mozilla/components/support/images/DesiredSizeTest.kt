/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images

import org.junit.Assert.assertEquals
import org.junit.Test

class DesiredSizeTest {

    @Test
    fun `minimum size is the same as supplied`() {
        val desiredSize = DesiredSize(128, 256, 512, 2.0f)

        assertEquals(desiredSize.targetSize, 128)
        assertEquals(desiredSize.minSize, 256)
        assertEquals(desiredSize.maxSize, 512)
        assertEquals(desiredSize.maxScaleFactor, 2.0f)
    }
}
