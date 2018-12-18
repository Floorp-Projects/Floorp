package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.widget.CheckBox
import mozilla.components.browser.menu.BrowserMenu
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SimpleBrowserMenuCheckboxTest {

    @Test
    fun `simple menu items are always visible by default`() {
        val item = SimpleBrowserMenuCheckbox("Hello") {
            // do nothing
        }

        assertTrue(item.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val item = SimpleBrowserMenuCheckbox("Hello") {
            // do nothing
        }

        val view = LayoutInflater.from(
            RuntimeEnvironment.application
        ).inflate(item.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `clicking bound view will invoke callback and dismiss menu`() {
        var callbackInvoked = false

        val item = SimpleBrowserMenuCheckbox("Hello") { checked ->
            callbackInvoked = checked
        }

        val menu = mock(BrowserMenu::class.java)
        val view = CheckBox(RuntimeEnvironment.application)

        item.bind(menu, view)

        view.isChecked = true

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }

    @Test
    fun `initialState is invoked on bind`() {
        val initialState: () -> Boolean = { true }
        val item = SimpleBrowserMenuCheckbox("Hello", initialState) {}

        val menu = mock(BrowserMenu::class.java)
        val view = spy(CheckBox(RuntimeEnvironment.application))
        item.bind(menu, view)

        verify(view).isChecked = true
    }
}