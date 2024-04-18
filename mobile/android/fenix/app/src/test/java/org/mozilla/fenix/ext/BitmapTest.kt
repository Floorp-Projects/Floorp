/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.view.View
import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Test

class BitmapTest {
    @Test
    fun `WHEN keyboard is considered closed THEN safeHeight returns the same value as the given view height`() {
        val view = mockk<View>(relaxed = true) {
            every { height } returns 100
            every { getKeyboardHeight() } returns 0
        }
        assertEquals(view.height, view.safeHeight())
    }

    @Test
    fun `WHEN keyboard is considered open THEN safeHeight returns the given view height plus the keyboard height`() {
        val view = mockk<View>(relaxed = true) {
            every { height } returns 100
            every { getKeyboardHeight() } returns 50
        }

        val expected = view.height.plus(view.getKeyboardHeight())
        assertEquals(expected, view.safeHeight())
    }
}
