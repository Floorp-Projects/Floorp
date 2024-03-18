/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import androidx.core.graphics.Insets
import androidx.core.view.WindowInsetsCompat
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class WindowInsetsCompatTest {
    private lateinit var windowInsetsCompat: WindowInsetsCompat
    private lateinit var insets: Insets
    private lateinit var mandatorySystemGestureInsets: Insets

    private var topPixels: Int = 0
    private var rightPixels: Int = 0
    private var leftPixels: Int = 0
    private var bottomPixels: Int = 0

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        windowInsetsCompat = mock()

        topPixels = 1
        rightPixels = 2
        leftPixels = 3
        bottomPixels = 4

        insets = Insets.of(leftPixels, topPixels, rightPixels, bottomPixels)
        mandatorySystemGestureInsets = Insets.of(leftPixels, topPixels, rightPixels, bottomPixels)

        whenever(windowInsetsCompat.getInsetsIgnoringVisibility(WindowInsetsCompat.Type.systemBars())).thenReturn(
            insets,
        )
        whenever(windowInsetsCompat.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures())).thenReturn(
            mandatorySystemGestureInsets,
        )
    }

    @Test
    fun testTop() {
        val topInsets = windowInsetsCompat.top()

        assertEquals(topPixels, topInsets)
    }

    @Test
    fun testRight() {
        val rightInsets = windowInsetsCompat.right()

        assertEquals(rightPixels, rightInsets)
    }

    @Test
    fun testLeft() {
        val leftInsets = windowInsetsCompat.left()

        assertEquals(leftPixels, leftInsets)
    }

    @Test
    fun testBottom() {
        val bottomInsets = windowInsetsCompat.bottom()

        assertEquals(bottomPixels, bottomInsets)
    }

    @Test
    fun testMandatorySystemGestureInsets() {
        val mandatorySystemGestureInsets = windowInsetsCompat.mandatorySystemGestureInsets()

        assertEquals(this.mandatorySystemGestureInsets, mandatorySystemGestureInsets)
    }
}
