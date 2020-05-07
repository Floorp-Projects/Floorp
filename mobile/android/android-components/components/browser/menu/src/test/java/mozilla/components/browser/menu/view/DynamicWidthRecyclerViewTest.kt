/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.util.AttributeSet
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class DynamicWidthRecyclerViewTest {

    @Test
    fun `minWidth and maxWidth should be initialized from xml attributes`() {
        val dynamicRecyclerView = DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 123, 456))

        assertEquals(123, dynamicRecyclerView.minWidth)
        assertEquals(456, dynamicRecyclerView.maxWidth)
    }

    @Test
    fun `If minWidth and maxWidth are not provided view should use layout_width`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100)))

        dynamicRecyclerView.measure(100, 100)

        // View should not try calculate/reconcile new dimensions, it should just call super.onMeasure(..)
        verify(dynamicRecyclerView).callParentOnMeasure(100, 100)
        verify(dynamicRecyclerView, never()).setReconciledDimensions(anyInt(), anyInt())
    }

    @Test
    fun `If only minWidth is provided view should use layout_width`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(
            testContext,
            getCustomAttributesSet(layoutWidth = 100, minWidth = 50)
        ))

        dynamicRecyclerView.measure(100, 100)

        // View should not try calculate/reconcile new dimensions, it should just call super.onMeasure(..)
        verify(dynamicRecyclerView).callParentOnMeasure(100, 100)
        verify(dynamicRecyclerView, never()).setReconciledDimensions(anyInt(), anyInt())
    }

    @Test
    fun `If only maxWidth is provided view should use layout_width`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(
            testContext,
            getCustomAttributesSet(layoutWidth = 100, maxWidth = 300)
        ))

        dynamicRecyclerView.measure(100, 100)

        // View should not try calculate/reconcile new dimensions, it should just call super.onMeasure(..)
        verify(dynamicRecyclerView).callParentOnMeasure(100, 100)
        verify(dynamicRecyclerView, never()).setReconciledDimensions(anyInt(), anyInt())
    }

    @Test
    fun `Should only allow for dynamic width if minWidth has a positive value`() {
        val dynamicRecyclerView = spy(
            DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, -1, 100)))

        dynamicRecyclerView.measure(1, 2)

        // If minWidth has a negative value we should only just call super.onMeasure(..)
        verify(dynamicRecyclerView).callParentOnMeasure(1, 2)
        verify(dynamicRecyclerView, never()).setReconciledDimensions(anyInt(), anyInt())
    }

    @Test
    fun `Should only allow for dynamic width if minWidth is smaller than maxWidth`() {
        val dynamicRecyclerView = spy(
            DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 100, 100)))

        dynamicRecyclerView.measure(1, 2)

        // If minWidth is >= to maxWidth we should only just call super.onMeasure(..)
        verify(dynamicRecyclerView).callParentOnMeasure(1, 2)
        verify(dynamicRecyclerView, never()).setReconciledDimensions(anyInt(), anyInt())
    }

    @Test
    fun `To allow for dynamic width children can expand entirely between minWidth and maxWidth`() {
        val dynamicRecyclerView = spy(
            DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 50, 100)))

        dynamicRecyclerView.measure(10, 10)

        // To allow for children to be as wide as they want widthSpec should be 0
        verify(dynamicRecyclerView).callParentOnMeasure(0, 10)
        // Robolectric doesn't do any kind of measuring and always returns 0 for View measurements.
        verify(dynamicRecyclerView).setReconciledDimensions(0, 0)
    }

    @Test
    @Config(qualifiers = "w333dp")
    fun `getScreenWidth() should return display's width in pixels`() {
        val dynamicRecyclerView = DynamicWidthRecyclerView(testContext, null)

        assertEquals(333, dynamicRecyclerView.getScreenWidth())
    }

    @Test
    fun `setReconciledDimensions() must set minWidth even if children width is smaller`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 50, 100)))

        dynamicRecyclerView.setReconciledDimensions(20, 100)

        verify(dynamicRecyclerView).callSetMeasuredDimension(50, 100)
    }

    @Test
    fun `setReconciledDimensions() will set children width if it is bigger than minWidth and smaller than maxWidth`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 50, 100)))

        dynamicRecyclerView.setReconciledDimensions(60, 100)

        verify(dynamicRecyclerView).callSetMeasuredDimension(60, 100)
    }

    @Test
    fun `setReconciledDimensions() must set maxWidth even if children width is bigger`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 50, 100)))

        dynamicRecyclerView.setReconciledDimensions(60, 100)

        verify(dynamicRecyclerView).callSetMeasuredDimension(60, 100)
    }

    @Test
    @Config(qualifiers = "w33dp")
    fun `setReconciledDimensions() must set the width to as much as screen width even if maxWidth is bigger`() {
        val dynamicRecyclerView = spy(DynamicWidthRecyclerView(testContext, getCustomAttributesSet(100, 50, 100)))

        dynamicRecyclerView.setReconciledDimensions(60, 100)

        verify(dynamicRecyclerView).callSetMeasuredDimension(33, 100)
    }

    private fun getCustomAttributesSet(layoutWidth: Int, minWidth: Int? = null, maxWidth: Int? = null): AttributeSet {
        return Robolectric.buildAttributeSet().apply {
            // android.R.attr.layout_width needs to always be set
            addAttribute(android.R.attr.layout_width, "${layoutWidth}dp")

            // R.attr.minWidth and R.attr.maxWidth are optional
            minWidth?.let { addAttribute(R.attr.minWidth, "${minWidth}dp") }
            maxWidth?.let { addAttribute(R.attr.maxWidth, "${maxWidth}dp") }
        }.build()
    }
}
