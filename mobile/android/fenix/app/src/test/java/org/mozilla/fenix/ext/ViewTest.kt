/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Rect
import android.os.Build
import android.util.DisplayMetrics
import android.view.View
import android.view.WindowInsets
import android.widget.FrameLayout
import androidx.core.view.WindowInsetsCompat
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(FenixRobolectricTestRunner::class)
class ViewTest {

    @MockK private lateinit var view: View
    @MockK private lateinit var parent: FrameLayout
    @MockK private lateinit var displayMetrics: DisplayMetrics

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        mockkStatic("org.mozilla.fenix.ext.ViewKt")

        every { view.resources.getDimensionPixelSize(any()) } answers {
            testContext.resources.getDimensionPixelSize(firstArg())
        }
        every { view.resources.displayMetrics } returns displayMetrics
        every { view.parent } returns parent
        every { parent.touchDelegate = any() } just Runs
        every { parent.post(any()) } answers {
            // Immediately run the given Runnable argument
            val action: Runnable = firstArg()
            action.run()
            true
        }
    }

    @Test
    fun `test increase touch area`() {
        val hitRect = Rect(30, 40, 50, 60)
        val dp = 10
        val px = 20
        val outRect = slot<Rect>()
        every { dp.dpToPx(displayMetrics) } returns px
        every { view.getHitRect(capture(outRect)) } answers { outRect.captured.set(hitRect) }

        view.increaseTapArea(dp)
        val expected = Rect(10, 20, 70, 80)
        assertEquals(expected.left, outRect.captured.left)
        assertEquals(expected.top, outRect.captured.top)
        assertEquals(expected.right, outRect.captured.right)
        assertEquals(expected.bottom, outRect.captured.bottom)
        verify { parent.touchDelegate = any() }
    }

    @Test
    fun `test remove touch delegate`() {
        view.removeTouchDelegate()
        verify { parent.touchDelegate = null }
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP, Build.VERSION_CODES.LOLLIPOP_MR1])
    @Test
    fun `getWindowInsets returns null below API 23`() {
        assertEquals(null, view.getWindowInsets())
    }

    @Test
    fun `getWindowInsets returns null when the system insets don't exist`() {
        every { view.rootWindowInsets } returns null
        assertEquals(null, view.getWindowInsets())
    }

    @Test
    fun `getWindowInsets returns the compat insets when the system insets exist`() {
        val rootInsets: WindowInsets = mockk(relaxed = true)
        every { view.rootWindowInsets } returns rootInsets

        assertEquals(WindowInsetsCompat.toWindowInsetsCompat(rootInsets), view.getWindowInsets())
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP, Build.VERSION_CODES.LOLLIPOP_MR1])
    @Test
    fun `getKeyboardHeight accounts for status bar below API 23`() {
        every { view.getWindowVisibleDisplayFrame() } returns Rect(0, 50, 1000, 500)
        every { view.rootView.height } returns 1000

        assertEquals(500, view.getKeyboardHeight())
    }

    @Test
    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19929
    fun `getKeyboardHeight accounts for status bar and navigation bar`() {
        every { view.getWindowVisibleDisplayFrame() } returns Rect(0, 50, 1000, 500)
        every { view.rootView.height } returns 1000
        every { view.getWindowInsets() } returns mockk(relaxed = true) {
            every { stableInsetBottom } returns 50
        }

        assertEquals(450, view.getKeyboardHeight())
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP, Build.VERSION_CODES.LOLLIPOP_MR1])
    @Test
    fun `isKeyboardVisible returns false when the keyboard height is less than or equal to the minimum threshold`() {
        val threshold = testContext.resources.getDimensionPixelSize(R.dimen.minimum_keyboard_height)

        every { view.getKeyboardHeight() } returns threshold - 1
        assertEquals(false, view.isKeyboardVisible())

        every { view.getKeyboardHeight() } returns threshold
        assertEquals(false, view.isKeyboardVisible())
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP, Build.VERSION_CODES.LOLLIPOP_MR1])
    @Test
    fun `isKeyboardVisible returns true when the keyboard height is greater than the minimum threshold`() {
        val threshold = testContext.resources.getDimensionPixelSize(R.dimen.minimum_keyboard_height)
        every { view.getKeyboardHeight() } returns threshold + 1

        assertEquals(true, view.isKeyboardVisible())
    }

    @Test
    fun `isKeyboardVisible returns false when the keyboard height is 0`() {
        every { view.getKeyboardHeight() } returns 0
        assertEquals(false, view.isKeyboardVisible())
    }

    @Test
    fun `isKeyboardVisible returns true when the keyboard height is greater than 0`() {
        every { view.getKeyboardHeight() } returns 100
        assertEquals(true, view.isKeyboardVisible())
    }

    @Test
    fun `getRectWithScreenLocation should transform getLocationInScreen method values`() {
        val locationOnScreen = slot<IntArray>()
        every { view.getLocationOnScreen(capture(locationOnScreen)) } answers {
            locationOnScreen.captured[0] = 100
            locationOnScreen.captured[1] = 200
        }
        every { view.width } returns 150
        every { view.height } returns 250

        val outRect = view.getRectWithScreenLocation()

        assertEquals(100, outRect.left)
        assertEquals(200, outRect.top)
        assertEquals(250, outRect.right)
        assertEquals(450, outRect.bottom)
    }
}
