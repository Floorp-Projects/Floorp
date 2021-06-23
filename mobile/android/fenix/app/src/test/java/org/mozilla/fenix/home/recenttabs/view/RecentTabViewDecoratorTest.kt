/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.graphics.drawable.Drawable
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToPx
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.R

class RecentTabViewDecoratorTest {
    @Test
    fun `WHEN forPosition is called with RecentTabsItemPosition#SINGLE THEN return SingleTabDecoration`() {
        val result = RecentTabViewDecorator.forPosition(RecentTabsItemPosition.SINGLE)

        assertTrue(result is RecentTabViewDecorator.SingleTabDecoration)
    }

    @Test
    fun `WHEN forPosition is called with RecentTabsItemPosition#TOP THEN return TopTabDecoration`() {
        val result = RecentTabViewDecorator.forPosition(RecentTabsItemPosition.TOP)

        assertTrue(result is RecentTabViewDecorator.TopTabDecoration)
    }

    @Test
    fun `WHEN forPosition is called with RecentTabsItemPosition#MIDDLE THEN return MiddleTabDecoration`() {
        val result = RecentTabViewDecorator.forPosition(RecentTabsItemPosition.MIDDLE)

        assertTrue(result is RecentTabViewDecorator.MiddleTabDecoration)
    }

    @Test
    fun `WHEN forPosition is called with RecentTabsItemPosition#BOTTOM THEN return SingleTabDecoration`() {
        val result = RecentTabViewDecorator.forPosition(RecentTabsItemPosition.BOTTOM)

        assertTrue(result is RecentTabViewDecorator.BottomTabDecoration)
    }

    @Test
    fun `WHEN SingleTabDecoration is invoked for a View THEN set the appropriate background`() {
        val view: View = mockk(relaxed = true)
        val drawable: Drawable = mockk()
        val drawableResCaptor = slot<Int>()

        try {
            mockkStatic(AppCompatResources::class)
            every { AppCompatResources.getDrawable(any(), capture(drawableResCaptor)) } returns drawable

            RecentTabViewDecorator.SingleTabDecoration(view)

            verify { view.background = drawable }
            assertEquals(R.drawable.home_list_row_background, drawableResCaptor.captured)
        } finally {
            unmockkStatic(AppCompatResources::class)
        }
    }

    @Test
    fun `WHEN TopTabDecoration is invoked for a View THEN set the appropriate background`() {
        val view: View = mockk(relaxed = true)
        val drawable: Drawable = mockk()
        val drawableResCaptor = slot<Int>()

        try {
            mockkStatic(AppCompatResources::class)
            every { AppCompatResources.getDrawable(any(), capture(drawableResCaptor)) } returns drawable

            RecentTabViewDecorator.TopTabDecoration(view)

            verify { view.background = drawable }
            assertEquals(R.drawable.rounded_top_corners, drawableResCaptor.captured)
        } finally {
            unmockkStatic(AppCompatResources::class)
        }
    }

    @Test
    fun `WHEN MiddleTabDecoration is invoked for a View THEN set the appropriate background and layout params`() {
        val colorAttrCaptor = slot<Int>()
        val viewLayoutParams: ViewGroup.MarginLayoutParams = mockk(relaxed = true)

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            mockkStatic("mozilla.components.support.ktx.android.content.ContextKt")
            val view: View = mockk(relaxed = true) {
                every { layoutParams } returns viewLayoutParams
                every { context.getColorFromAttr(capture(colorAttrCaptor)) } returns 42
                every { context.resources.displayMetrics } returns mockk(relaxed = true)
            }
            every { any<Int>().dpToPx(any()) } returns 43

            RecentTabViewDecorator.MiddleTabDecoration(view)

            verify { view.setBackgroundColor(42) }
            assertEquals(R.attr.above, colorAttrCaptor.captured)
            assertEquals(43, viewLayoutParams.topMargin)
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.content.ContextKt")
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }
    }

    @Test
    fun `WHEN BottomTabDecoration is invoked for a View THEN set the appropriate background and layout params`() {
        val viewLayoutParams: ViewGroup.MarginLayoutParams = mockk(relaxed = true)
        val drawable: Drawable = mockk()
        val drawableResCaptor = slot<Int>()

        try {
            mockkStatic(AppCompatResources::class)
            every { AppCompatResources.getDrawable(any(), capture(drawableResCaptor)) } returns drawable
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            val view: View = mockk(relaxed = true) {
                every { layoutParams } returns viewLayoutParams
                every { context.resources.displayMetrics } returns mockk(relaxed = true)
            }
            every { any<Int>().dpToPx(any()) } returns 43

            RecentTabViewDecorator.BottomTabDecoration(view)

            verify { view.background = drawable }
            assertEquals(R.drawable.rounded_bottom_corners, drawableResCaptor.captured)
            assertEquals(43, viewLayoutParams.topMargin)
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }
    }
}
