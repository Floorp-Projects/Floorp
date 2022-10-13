/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import android.content.res.Resources
import android.graphics.Bitmap
import android.util.DisplayMetrics
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.images.DesiredSize
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ResizingProcessorTest {
    private lateinit var processor: ResizingProcessor

    @Before
    fun setup() {
        processor = spy(ResizingProcessor())
    }

    @Test
    fun `icons of the correct size are not resized`() {
        val icon = Icon(mockBitmap(64), source = Icon.Source.DISK)
        val resized = process(icon = icon, desiredSize = DesiredSize(64, 64, 100, 3f))

        assertEquals(icon.bitmap, resized?.bitmap)

        verify(processor, never()).resize(any(), anyInt())
    }

    @Test
    fun `larger icons are resized`() {
        val icon = Icon(mockBitmap(120), source = Icon.Source.DISK)
        val smallerIcon = mockBitmap(64)
        doReturn(smallerIcon).`when`(processor).resize(eq(icon.bitmap), anyInt())
        val resized = process(icon = icon, desiredSize = DesiredSize(64, 64, 64, 3f))

        assertNotEquals(icon.bitmap, resized?.bitmap)
        assertEquals(smallerIcon, resized?.bitmap)

        verify(processor).resize(icon.bitmap, 64)
    }

    @Test
    fun `smaller icons are resized`() {
        val icon = Icon(mockBitmap(34), source = Icon.Source.DISK)
        val largerIcon = mockBitmap(64)
        doReturn(largerIcon).`when`(processor).resize(eq(icon.bitmap), anyInt())
        val resized = process(icon = icon, desiredSize = DesiredSize(64, 64, 70, 3f))

        assertNotEquals(icon.bitmap, resized?.bitmap)
        assertEquals(largerIcon, resized?.bitmap)

        verify(processor).resize(icon.bitmap, 64)
    }

    @Test
    fun `smaller icons are not resized past the max scale factor`() {
        val icon = Icon(mockBitmap(10), source = Icon.Source.DISK)
        val largerIcon = mockBitmap(64)
        doReturn(largerIcon).`when`(processor).resize(eq(icon.bitmap), anyInt())
        val resized = process(icon = icon)

        assertNotEquals(icon.bitmap, resized?.bitmap)
        assertNull(resized)
    }

    @Test
    fun `smaller icons are resized to the max scale factor if permitted`() {
        val processor = spy(ResizingProcessor(discardSmallIcons = false))

        val icon = Icon(mockBitmap(10), source = Icon.Source.DISK)
        val largerIcon = mockBitmap(64)
        doReturn(largerIcon).`when`(processor).resize(eq(icon.bitmap), anyInt())
        val resized = process(processor, icon = icon)

        assertNotEquals(icon.bitmap, resized?.bitmap)
        assertNotNull(resized)

        verify(processor).resize(icon.bitmap, 30)
    }

    private fun process(
        p: ResizingProcessor = processor,
        context: Context = mockContext(2f),
        request: IconRequest = IconRequest("https://mozilla.org"),
        resource: IconRequest.Resource = mock(),
        icon: Icon = Icon(mockBitmap(64), source = Icon.Source.DISK),
        desiredSize: DesiredSize = DesiredSize(96, 96, 1000, 3f),
    ) = p.process(context, request, resource, icon, desiredSize)

    private fun mockContext(density: Float): Context {
        val context: Context = mock()
        val resources: Resources = mock()
        val displayMetrics = DisplayMetrics()
        doReturn(resources).`when`(context).resources
        doReturn(displayMetrics).`when`(resources).displayMetrics
        displayMetrics.density = density
        return context
    }

    private fun mockBitmap(size: Int): Bitmap {
        val bitmap: Bitmap = mock()
        doReturn(size).`when`(bitmap).height
        doReturn(size).`when`(bitmap).width
        return bitmap
    }
}
