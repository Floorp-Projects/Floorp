/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import java.lang.IllegalArgumentException

class ExceptionTest {

    @Test
    fun uniqueId() {
        val id = IllegalArgumentException("test").uniqueId()
        val id2 = IllegalArgumentException("test2").uniqueId()
        assertNotEquals(id, id2)

        val ids = (0..1).map { IllegalArgumentException("test").uniqueId() }
        assertEquals(2, ids.size)
        assertEquals(ids[0], ids[1])
    }
}
