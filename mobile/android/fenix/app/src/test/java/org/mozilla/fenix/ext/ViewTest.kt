/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Rect
import android.util.DisplayMetrics
import android.view.View
import android.widget.FrameLayout
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import mozilla.components.support.ktx.android.util.dpToPx
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class ViewTest {

    @MockK private lateinit var view: View
    @MockK private lateinit var parent: FrameLayout
    @MockK private lateinit var displayMetrics: DisplayMetrics

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")

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
}
