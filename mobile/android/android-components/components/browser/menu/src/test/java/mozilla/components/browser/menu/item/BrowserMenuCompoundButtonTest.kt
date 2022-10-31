/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.view.View
import android.view.ViewTreeObserver
import android.widget.CheckBox
import androidx.appcompat.widget.SwitchCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserMenuCompoundButtonTest {

    @Test
    fun `simple menu items are always visible by default`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {
            // do nothing
        }

        assertTrue(item.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {
            // do nothing
        }

        val view = LayoutInflater.from(testContext)
            .inflate(item.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `clicking bound view will invoke callback and dismiss menu`() {
        var callbackInvoked = false

        val item = SimpleTestBrowserCompoundButton("Hello") { checked ->
            callbackInvoked = checked
        }

        val menu = mock(BrowserMenu::class.java)
        val view = CheckBox(testContext)

        item.bind(menu, view)

        view.isChecked = true

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }

    @Test
    fun `initialState is invoked on bind`() {
        val initialState: () -> Boolean = { true }
        val item = SimpleTestBrowserCompoundButton("Hello", initialState) {}

        val menu = mock(BrowserMenu::class.java)
        val view = spy(CheckBox(testContext))
        item.bind(menu, view)

        verify(view).isChecked = true
    }

    @Test
    fun `hitting default methods`() {
        val item = SimpleTestBrowserCompoundButton("") {}
        item.invalidate(mock(View::class.java))
    }

    @Test
    fun `menu compound button can be converted to candidate`() {
        val listener = { _: Boolean -> }

        assertEquals(
            CompoundMenuCandidate(
                "Hello",
                isChecked = false,
                end = CompoundMenuCandidate.ButtonType.CHECKBOX,
                onCheckedChange = listener,
            ),
            SimpleTestBrowserCompoundButton(
                "Hello",
                listener = listener,
            ).asCandidate(testContext),
        )

        assertEquals(
            CompoundMenuCandidate(
                "Hello",
                isChecked = true,
                end = CompoundMenuCandidate.ButtonType.CHECKBOX,
                onCheckedChange = listener,
            ),
            SimpleTestBrowserCompoundButton(
                "Hello",
                initialState = { true },
                listener = listener,
            ).asCandidate(testContext),
        )
    }

    @Test
    fun `GIVEN the View is attached to Window WHEN bind is called THEN the layout direction is not updated`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {}
        val menu = mock(BrowserMenu::class.java)
        val view: SwitchCompat = mock()
        doReturn(true).`when`(view).isAttachedToWindow
        doReturn(mock<ViewTreeObserver>()).`when`(view).viewTreeObserver

        item.bind(menu, view)

        verify(view, never()).layoutDirection = ArgumentMatchers.anyInt()
    }

    @Test
    fun `GIVEN the View is not attached to Window WHEN bind is called THEN the layout direction is changed to locale`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {}
        val menu = mock(BrowserMenu::class.java)
        val view: SwitchCompat = mock()
        doReturn(false).`when`(view).isAttachedToWindow
        doReturn(mock<ViewTreeObserver>()).`when`(view).viewTreeObserver

        item.bind(menu, view)

        verify(view).layoutDirection = View.LAYOUT_DIRECTION_LOCALE
    }

    @Test
    fun `GIVEN the View is not attached to Window WHEN bind is called THEN the a viewTreeObserver for preDraw is set`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {}
        val menu = mock(BrowserMenu::class.java)
        val view: SwitchCompat = mock()
        val viewTreeObserver: ViewTreeObserver = mock()
        doReturn(false).`when`(view).isAttachedToWindow
        doReturn(viewTreeObserver).`when`(view).viewTreeObserver

        item.bind(menu, view)

        verify(viewTreeObserver).addOnPreDrawListener(any())
    }

    @Test
    fun `GIVEN a view with updated layout direction WHEN it is about to be drawn THEN the layout direction reset`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {}
        val menu = mock(BrowserMenu::class.java)
        val view: SwitchCompat = mock()
        val viewTreeObserver: ViewTreeObserver = mock()
        doReturn(false).`when`(view).isAttachedToWindow
        doReturn(viewTreeObserver).`when`(view).viewTreeObserver
        val captor = argumentCaptor<ViewTreeObserver.OnPreDrawListener>()

        item.bind(menu, view)
        verify(viewTreeObserver).addOnPreDrawListener(captor.capture())

        captor.value.onPreDraw()
        verify(viewTreeObserver).removeOnPreDrawListener(captor.value)
        verify(view).layoutDirection = View.LAYOUT_DIRECTION_INHERIT
    }

    class SimpleTestBrowserCompoundButton(
        label: String,
        initialState: () -> Boolean = { false },
        listener: (Boolean) -> Unit,
    ) : BrowserMenuCompoundButton(label, false, false, initialState, listener) {
        override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_item_simple
    }
}
