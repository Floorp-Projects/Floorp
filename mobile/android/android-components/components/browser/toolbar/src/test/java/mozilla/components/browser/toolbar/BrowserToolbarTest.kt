/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import android.view.View
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.browser.toolbar.edit.EditToolbar
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class BrowserToolbarTest {
    @Test
    fun `display toolbar is visible by default`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `calling editMode() makes edit toolbar visible`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)

        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)
    }

    @Test
    fun `calling displayMode() makes display toolbar visible`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)

        toolbar.displayMode()

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `back presses will not be handled in display mode`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        toolbar.displayMode()

        assertFalse(toolbar.onBackPressed())

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `back presses will switch from edit mode to display mode`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)

        assertTrue(toolbar.onBackPressed())

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `displayUrl will be forwarded to display toolbar immediately`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)
        val ediToolbar = mock(EditToolbar::class.java)

        toolbar.displayToolbar = displayToolbar
        toolbar.editToolbar = ediToolbar

        toolbar.url = "https://www.mozilla.org"

        verify(displayToolbar).updateUrl("https://www.mozilla.org")
        verify(ediToolbar, never()).updateUrl(ArgumentMatchers.anyString())
    }

    @Test
    fun `last URL will be forwarded to edit toolbar when switching mode`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        val ediToolbar = mock(EditToolbar::class.java)
        toolbar.editToolbar = ediToolbar

        toolbar.url = "https://www.mozilla.org"
        verify(ediToolbar, never()).updateUrl("https://www.mozilla.org")

        toolbar.editMode()

        verify(ediToolbar).updateUrl("https://www.mozilla.org")
    }

    @Test
    fun `displayProgress will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)

        toolbar.displayToolbar = displayToolbar

        toolbar.displayProgress(10)
        toolbar.displayProgress(50)
        toolbar.displayProgress(75)
        toolbar.displayProgress(100)

        verify(displayToolbar).updateProgress(10)
        verify(displayToolbar).updateProgress(50)
        verify(displayToolbar).updateProgress(75)
        verify(displayToolbar).updateProgress(100)

        verifyNoMoreInteractions(displayToolbar)
    }

    @Test
    fun `internal onUrlEntered callback will be forwarded to urlChangeListener`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        val mockedListener = object {
            var called = false
            var url: String? = null

            fun invoke(url: String) {
                this.called = true
                this.url = url
            }
        }

        toolbar.setOnUrlCommitListener(mockedListener::invoke)
        toolbar.onUrlEntered("https://www.mozilla.org")

        assertTrue(mockedListener.called)
        assertEquals("https://www.mozilla.org", mockedListener.url)
    }

    @Test
    fun `toolbar measure will use full width and fixed 56dp height`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.AT_MOST)

        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, toolbar.measuredWidth)
        assertEquals(56, toolbar.measuredHeight)
    }

    @Test
    fun `toolbar will use provided height with EXACTLY measure spec`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY)

        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, toolbar.measuredWidth)
        assertEquals(800, toolbar.measuredHeight)
    }

    @Test
    fun `display and edit toolbar will use full size of browser toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        assertEquals(0, toolbar.displayToolbar.measuredWidth)
        assertEquals(0, toolbar.displayToolbar.measuredHeight)
        assertEquals(0, toolbar.editToolbar.measuredWidth)
        assertEquals(0, toolbar.editToolbar.measuredHeight)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.AT_MOST)

        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, toolbar.displayToolbar.measuredWidth)
        assertEquals(56, toolbar.displayToolbar.measuredHeight)
        assertEquals(1024, toolbar.editToolbar.measuredWidth)
        assertEquals(56, toolbar.editToolbar.measuredHeight)
    }

    fun `toolbar will switch back to display mode after an URL has been entered`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)

        toolbar.onUrlEntered("https://www.mozilla.org")

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `display and edit toolbar will be laid out at the exact same position`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)
        val editToolbar = mock(EditToolbar::class.java)

        toolbar.displayToolbar = displayToolbar
        toolbar.editToolbar = editToolbar

        toolbar.removeAllViews()
        toolbar.addView(toolbar.displayToolbar)
        toolbar.addView(toolbar.editToolbar)

        `when`(displayToolbar.measuredWidth).thenReturn(1000)
        `when`(displayToolbar.measuredHeight).thenReturn(200)
        `when`(editToolbar.measuredWidth).thenReturn(1000)
        `when`(editToolbar.measuredHeight).thenReturn(200)

        toolbar.layout(100, 100, 1100, 300)

        verify(displayToolbar).layout(0, 0, 1000, 200)
        verify(editToolbar).layout(0, 0, 1000, 200)
    }

    @Test
    fun `menu builder will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        assertNull(toolbar.displayToolbar.menuBuilder)

        val menuBuilder = BrowserMenuBuilder(emptyList())
        toolbar.setMenuBuilder(menuBuilder)

        assertNotNull(toolbar.displayToolbar.menuBuilder)
        assertEquals(menuBuilder, toolbar.displayToolbar.menuBuilder)
    }

    @Test
    fun `add browser action will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)

        toolbar.displayToolbar = displayToolbar

        val action = BrowserToolbar.Button(0, "Hello") {
            // Do nothing
        }

        toolbar.addBrowserAction(action)

        verify(displayToolbar).addBrowserAction(action)
    }

    @Test
    fun `add page action will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        val displayToolbar = mock(DisplayToolbar::class.java)

        toolbar.displayToolbar = displayToolbar

        val action = BrowserToolbar.Button(0, "World") {
            // Do nothing
        }

        toolbar.addPageAction(action)

        verify(displayToolbar).addPageAction(action)
    }

    fun `URL update does not override search terms in edit mode`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)
        val editToolbar = mock(EditToolbar::class.java)

        toolbar.displayToolbar = displayToolbar
        toolbar.editToolbar = editToolbar

        toolbar.setSearchTerms("mozilla android")
        toolbar.url = "https://www.mozilla.org"
        toolbar.editMode()
        verify(displayToolbar).updateUrl("https://www.mozilla.org")
        verify(editToolbar).updateUrl("mozilla android")

        toolbar.setSearchTerms("")
        toolbar.url = "https://www.mozilla.org"
        toolbar.editMode()
        verify(displayToolbar).updateUrl("https://www.mozilla.org")
        verify(editToolbar).updateUrl("https://www.mozilla.org")
    }

    @Test
    fun `add navigation action will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)
        toolbar.displayToolbar = displayToolbar

        val action = BrowserToolbar.Button(0, "Back") {
            // Do nothing
        }

        toolbar.addNavigationAction(action)

        verify(displayToolbar).addNavigationAction(action)
    }

    @Test
    fun `invalidate actions is forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = mock(DisplayToolbar::class.java)
        toolbar.displayToolbar = displayToolbar

        verify(displayToolbar, never()).invalidateActions()

        toolbar.invalidateActions()

        verify(displayToolbar).invalidateActions()
    }

    @Test
    fun `search terms (if set) are forwarded to edit toolbar instead of URL`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)

        val ediToolbar = mock(EditToolbar::class.java)
        toolbar.editToolbar = ediToolbar

        toolbar.url = "https://www.mozilla.org"
        toolbar.setSearchTerms("Mozilla Firefox")

        verify(ediToolbar, never()).updateUrl("https://www.mozilla.org")
        verify(ediToolbar, never()).updateUrl("Mozilla Firefox")

        toolbar.editMode()

        verify(ediToolbar, never()).updateUrl("https://www.mozilla.org")
        verify(ediToolbar).updateUrl("Mozilla Firefox")
    }

    @Test
    fun `urlBoxBackgroundDrawable, browserActionMargin and urlBoxMargin are forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = toolbar.displayToolbar

        assertNull(displayToolbar.urlBoxView)
        assertEquals(displayToolbar.browserActionMargin, 0)
        assertEquals(displayToolbar.urlBoxMargin, 0)

        val view = mock(View::class.java)
        toolbar.urlBoxView = view
        toolbar.browserActionMargin = 42
        toolbar.urlBoxMargin = 23

        assertEquals(view, displayToolbar.urlBoxView)
        assertEquals(42, displayToolbar.browserActionMargin)
        assertEquals(23, displayToolbar.urlBoxMargin)
    }

    @Test
    fun `onUrlClicked is forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val displayToolbar = toolbar.displayToolbar

        assertTrue(displayToolbar.onUrlClicked())

        toolbar.onUrlClicked = { false }

        assertFalse(displayToolbar.onUrlClicked())
    }
}
