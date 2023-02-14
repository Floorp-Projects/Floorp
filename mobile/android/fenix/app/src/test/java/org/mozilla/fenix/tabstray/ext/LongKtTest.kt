/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import org.junit.Assert.assertEquals
import org.junit.Test

class LongKtTest {

    @Test
    fun `WHEN value is null THEN default is returned`() {
        val value: Long? = null

        assertEquals(value.orDefault(), -1L)
    }

    @Test
    fun `WHEN value is not null THEN value is returned`() {
        val value: Long? = 100L

        assertEquals(value.orDefault(), 100L)
    }
}
