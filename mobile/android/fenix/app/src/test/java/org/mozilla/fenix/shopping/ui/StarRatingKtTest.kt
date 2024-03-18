/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import junit.framework.TestCase.assertEquals
import org.junit.Test

class StarRatingKtTest {

    @Test
    fun `GIVEN a float with a decimal THEN remove the zero if present`() {
        assertEquals(3, 3.0f.removeDecimalZero())
        assertEquals(4.5f, 4.5f.removeDecimalZero())
        assertEquals(5, 5.0f.removeDecimalZero())
        assertEquals(0.2f, 0.2f.removeDecimalZero())
        assertEquals(1.00001f, 1.00001f.removeDecimalZero())
        assertEquals(4.999f, 4.999f.removeDecimalZero())
    }

    @Test
    fun `GIVEN a float with a decimal THEN round its value to the nearest half`() {
        assertEquals(3.0f, 3.1f.roundToNearestHalf())
        assertEquals(4.5f, 4.6f.roundToNearestHalf())
        assertEquals(5.0f, 4.9f.roundToNearestHalf())
        assertEquals(0.5f, 0.3f.roundToNearestHalf())
        assertEquals(2.5f, 2.26f.roundToNearestHalf())
        assertEquals(2.0f, 2.25f.roundToNearestHalf())
    }
}
