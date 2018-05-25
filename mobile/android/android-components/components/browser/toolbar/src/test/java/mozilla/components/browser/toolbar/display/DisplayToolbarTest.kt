/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.view.View
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.ktx.android.view.forEach
import mozilla.components.ui.progress.AnimatedProgressBar
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class DisplayToolbarTest {
    @Test
    fun `clicking on the URL switches the toolbar to editing mode`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val urlView = extractUrlView(displayToolbar)
        assertTrue(urlView.performClick())

        verify(toolbar).editMode()
    }

    @Test
    fun `progress is forwarded to progress bar`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val progressView = extractProgressView(displayToolbar)

        displayToolbar.updateProgress(10)
        assertEquals(10, progressView.progress)

        displayToolbar.updateProgress(50)
        assertEquals(50, progressView.progress)

        displayToolbar.updateProgress(75)
        assertEquals(75, progressView.progress)

        displayToolbar.updateProgress(100)
        assertEquals(100, progressView.progress)
    }

    @Test
    fun `icon view will use square size`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        displayToolbar.measure(widthSpec, heightSpec)

        val iconView = extractIconView(displayToolbar)

        assertEquals(56, iconView.measuredWidth)
        assertEquals(56, iconView.measuredHeight)
    }

    @Test
    fun `progress view will use full width and 3dp height`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        displayToolbar.measure(widthSpec, heightSpec)

        val progressView = extractProgressView(displayToolbar)

        assertEquals(1024, progressView.measuredWidth)
        assertEquals(3, progressView.measuredHeight)
    }

    @Test
    fun `menu view is gone by default`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val menuView = extractMenuView(displayToolbar)
        assertNotNull(menuView)
        assertTrue(menuView.visibility == View.GONE)
    }

    @Test
    fun `menu view becomes visible once a menu builder is set`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val menuView = extractMenuView(displayToolbar)
        assertNotNull(menuView)

        assertTrue(menuView.visibility == View.GONE)

        displayToolbar.menuBuilder = BrowserMenuBuilder(emptyList())

        assertTrue(menuView.visibility == View.VISIBLE)

        displayToolbar.menuBuilder = null

        assertTrue(menuView.visibility == View.GONE)
    }

    @Test
    fun `no menu builder is set by default`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        assertNull(displayToolbar.menuBuilder)
    }

    @Test
    fun `menu builder will be used to create and show menu when button is clicked`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)
        val menuView = extractMenuView(displayToolbar)

        val menuBuilder = mock(BrowserMenuBuilder::class.java)
        val menu = mock(BrowserMenu::class.java)
        doReturn(menu).`when`(menuBuilder).build(RuntimeEnvironment.application)

        displayToolbar.menuBuilder = menuBuilder

        verify(menuBuilder, never()).build(RuntimeEnvironment.application)
        verify(menu, never()).show(menuView)

        menuView.performClick()

        verify(menuBuilder).build(RuntimeEnvironment.application)
        verify(menu).show(menuView)
    }

    @Test
    fun `browser action gets added as view to toolbar`() {
        val contentDescription = "Mozilla"

        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        assertNull(extractActionView(displayToolbar, contentDescription))

        val action = Toolbar.Action(0, contentDescription) {}
        displayToolbar.addBrowserAction(action)

        val view = extractActionView(displayToolbar, contentDescription)
        assertNotNull(view)
        assertEquals(contentDescription, view?.contentDescription)
    }

    @Test
    fun `browser actions do not get added as view if limit is reached`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        displayToolbar.addBrowserAction(Toolbar.Action(0, "Action-1") {})
        displayToolbar.addBrowserAction(Toolbar.Action(0, "Action-2") {})
        displayToolbar.addBrowserAction(Toolbar.Action(0, "Action-3") {})

        assertNotNull(extractActionView(displayToolbar, "Action-1"))
        assertNotNull(extractActionView(displayToolbar, "Action-2"))

        assertNull(extractActionView(displayToolbar, "Action-3"))
    }

    @Test
    fun `clicking browser action view triggers listener of action`() {
        var callbackExecuted = false

        val action = Toolbar.Action(0, "Button") {
            callbackExecuted = true
        }

        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)
        displayToolbar.addBrowserAction(action)

        val view = extractActionView(displayToolbar, "Button")
        assertNotNull(view)

        assertFalse(callbackExecuted)

        view?.performClick()

        assertTrue(callbackExecuted)
    }

    @Test
    fun `browser action view will use square size`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        val action = Toolbar.Action(0, "action") {}
        displayToolbar.addBrowserAction(action)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        displayToolbar.measure(widthSpec, heightSpec)

        val view = extractActionView(displayToolbar, "action")!!

        assertEquals(56, view.measuredWidth)
        assertEquals(56, view.measuredHeight)
    }

    @Test
    fun `page actions will be added as view to the toolbar`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)

        assertNull(extractActionView(displayToolbar, "Reader Mode"))

        val action = Toolbar.Action(0, "Reader Mode") {}
        displayToolbar.addPageAction(action)

        assertNotNull(extractActionView(displayToolbar, "Reader Mode"))
    }

    @Test
    fun `clicking a page action view will execute the listener of the action`() {
        var listenerExecuted = false

        val action = Toolbar.Action(0, "Reload") {
            listenerExecuted = true
        }

        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)
        displayToolbar.addPageAction(action)

        assertFalse(listenerExecuted)

        val view = extractActionView(displayToolbar, "Reload")
        assertNotNull(view)
        view!!.performClick()

        assertTrue(listenerExecuted)
    }

    fun `views for page actions will have a square shape`() {
        val action = Toolbar.Action(0, "Open app") {}

        val toolbar = mock(BrowserToolbar::class.java)
        val displayToolbar = DisplayToolbar(RuntimeEnvironment.application, toolbar)
        displayToolbar.addPageAction(action)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        displayToolbar.measure(widthSpec, heightSpec)

        val view = extractActionView(displayToolbar, "Open app")!!

        assertEquals(56, view.measuredWidth)
        assertEquals(56, view.measuredHeight)
    }

    companion object {
        private fun extractUrlView(displayToolbar: DisplayToolbar): TextView {
            var textView: TextView? = null

            displayToolbar.forEach {
                if (it is TextView) {
                    textView = it
                    return@forEach
                }
            }

            return textView ?: throw AssertionError("Could not find URL view")
        }

        private fun extractProgressView(displayToolbar: DisplayToolbar): AnimatedProgressBar {
            var progressView: AnimatedProgressBar? = null

            displayToolbar.forEach {
                if (it is AnimatedProgressBar) {
                    progressView = it
                    return@forEach
                }
            }

            return progressView ?: throw AssertionError("Could not find URL view")
        }

        private fun extractIconView(displayToolbar: DisplayToolbar): ImageView {
            var iconView: ImageView? = null

            displayToolbar.forEach {
                if (it is ImageView) {
                    iconView = it
                    return@forEach
                }
            }

            return iconView ?: throw AssertionError("Could not find URL view")
        }

        private fun extractMenuView(displayToolbar: DisplayToolbar): ImageButton {
            var menuButton: ImageButton? = null

            displayToolbar.forEach {
                if (it is ImageButton) {
                    menuButton = it
                    return@forEach
                }
            }

            return menuButton ?: throw AssertionError("Could not find menu view")
        }

        private fun extractActionView(
            displayToolbar: DisplayToolbar,
            contentDescription: String
        ): ImageButton? {
            var actionView: ImageButton? = null

            displayToolbar.forEach {
                if (it is ImageButton && it.contentDescription == contentDescription) {
                    actionView = it
                    return@forEach
                }
            }

            return actionView
        }
    }
}
