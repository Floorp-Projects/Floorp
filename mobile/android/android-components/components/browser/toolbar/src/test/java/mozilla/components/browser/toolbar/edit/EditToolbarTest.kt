/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.content.Context
import android.view.KeyEvent
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.CountDownLatch

@RunWith(RobolectricTestRunner::class)
class EditToolbarTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `entered text is forwarded to async autocomplete filter`() {
        val toolbar = BrowserToolbar(context)
        toolbar.editToolbar.urlView.onAttachedToWindow()

        val latch = CountDownLatch(1)
        var invokedWithParams: List<Any?>? = null
        toolbar.setAutocompleteListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
            latch.countDown()
        }

        toolbar.editToolbar.urlView.setText("Hello")

        // Autocomplete filter will be invoked on a worker thread.
        // Serialize here for the sake of tests.
        runBlocking {
            latch.await()
        }

        assertEquals("Hello", invokedWithParams!![0])
        assertTrue(invokedWithParams!![1] is AutocompleteDelegate)
    }

    @Test
    fun `focus change is forwarded to listener`() {
        var listenerInvoked = false
        var value = false

        val toolbar = BrowserToolbar(context)
        toolbar.setOnEditFocusChangeListener { hasFocus ->
            listenerInvoked = true
            value = hasFocus
        }

        // Switch to editing mode and focus view.
        toolbar.editMode()
        toolbar.editToolbar.urlView.requestFocus()

        assertTrue(listenerInvoked)
        assertTrue(value)

        // Switch back to display mode
        listenerInvoked = false
        toolbar.displayMode()

        assertTrue(listenerInvoked)
        assertFalse(value)
    }

    @Test
    fun `entering text emits commit fact`() {
        CollectionProcessor.withFactCollection { facts ->
            val toolbar = BrowserToolbar(context)
            toolbar.editToolbar.urlView.onAttachedToWindow()

            assertEquals(0, facts.size)

            toolbar.editToolbar.urlView.setText("https://www.mozilla.org")
            toolbar.editToolbar.urlView.dispatchKeyEvent(KeyEvent(
                System.currentTimeMillis(),
                System.currentTimeMillis(),
                KeyEvent.ACTION_DOWN,
                KeyEvent.KEYCODE_ENTER,
                0))

            assertEquals(1, facts.size)

            val fact = facts[0]
            assertEquals(Component.BROWSER_TOOLBAR, fact.component)
            assertEquals(Action.COMMIT, fact.action)
            assertEquals("toolbar", fact.item)
            assertNull(fact.value)
            assertNull(fact.metadata)
        }
    }
}
