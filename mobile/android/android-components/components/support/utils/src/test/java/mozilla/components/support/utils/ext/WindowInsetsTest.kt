/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.view.WindowInsets
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert
import org.junit.Before
import org.junit.Test

class WindowInsetsTest {
    private lateinit var insets: WindowInsets
    private var topPixels: Int = 0
    private var rightPixels: Int = 0
    private var leftPixels: Int = 0
    private var bottomPixels: Int = 0

    @Before
    @ExperimentalCoroutinesApi
    @Suppress("DEPRECATION")
    fun setUp() {
        insets = mock()
        topPixels = 1
        rightPixels = 2
        leftPixels = 3
        bottomPixels = 4
        whenever(insets.systemWindowInsetTop).thenReturn(topPixels)
        whenever(insets.systemWindowInsetRight).thenReturn(rightPixels)
        whenever(insets.systemWindowInsetLeft).thenReturn(leftPixels)
        whenever(insets.systemWindowInsetBottom).thenReturn(bottomPixels)
    }

    @Test
    fun testTop() {
        val topInsets = insets.top()

        Assert.assertEquals(topPixels, topInsets)
    }

    @Test
    fun testRight() {
        val rightInsets = insets.right()

        Assert.assertEquals(rightPixels, rightInsets)
    }

    @Test
    fun testLeft() {
        val leftInsets = insets.left()

        Assert.assertEquals(leftPixels, leftInsets)
    }

    @Test
    fun testBottom() {
        val bottomInsets = insets.bottom()

        Assert.assertEquals(bottomPixels, bottomInsets)
    }
}
