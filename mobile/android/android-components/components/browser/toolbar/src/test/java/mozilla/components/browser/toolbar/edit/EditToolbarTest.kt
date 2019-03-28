/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.content.Context
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.ktx.android.view.forEach
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
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

            val metadata = fact.metadata
            assertNotNull(metadata!!)
            assertEquals(1, metadata.size)
            assertTrue(metadata.contains("autocomplete"))
            assertTrue(metadata["autocomplete"] is Boolean)
            assertFalse(metadata["autocomplete"] as Boolean)
        }
    }

    @Test
    fun `entering text emits commit fact with autocomplete metadata`() {
        CollectionProcessor.withFactCollection { facts ->
            val toolbar = BrowserToolbar(context)
            toolbar.editToolbar.urlView.onAttachedToWindow()

            assertEquals(0, facts.size)

            toolbar.editToolbar.urlView.setText("https://www.mozilla.org")

            // Fake autocomplete
            toolbar.editToolbar.urlView.autocompleteResult = InlineAutocompleteEditText.AutocompleteResult(
                text = "hello world",
                source = "test-source",
                totalItems = 100)

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

            val metadata = fact.metadata
            assertNotNull(metadata!!)
            assertEquals(2, metadata.size)

            assertTrue(metadata.contains("autocomplete"))
            assertTrue(metadata["autocomplete"] is Boolean)
            assertTrue(metadata["autocomplete"] as Boolean)

            assertTrue(metadata.contains("source"))
            assertEquals("test-source", metadata["source"])
        }
    }

    @Test
    fun `clearView clears text in urlView`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val editToolbar = EditToolbar(context, toolbar)
        val clearView = extractClearView(editToolbar)

        editToolbar.urlView.setText("https://www.mozilla.org")
        assertTrue(editToolbar.urlView.text.isNotBlank())

        Assert.assertNotNull(clearView)
        clearView.performClick()
        assertTrue(editToolbar.urlView.text.isBlank())
    }

    @Test
    fun `fun updateClearViewVisibility updates clearView`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val editToolbar = EditToolbar(context, toolbar)
        val clearView = extractClearView(editToolbar)

        editToolbar.updateClearViewVisibility("")
        assertTrue(clearView.visibility == View.GONE)

        editToolbar.updateClearViewVisibility("https://www.mozilla.org")
        assertTrue(clearView.visibility == View.VISIBLE)
    }

    @Test
    fun `clearView changes image color filter on update`() {
        val toolbar = BrowserToolbar(context)
        val editToolbar = toolbar.editToolbar
        editToolbar.clearViewColor = R.color.photonBlue40

        assertEquals(R.color.photonBlue40, editToolbar.clearViewColor)
    }

    companion object {
        private fun extractClearView(editToolbar: EditToolbar): ImageView {
            var clearView: ImageView? = null

            editToolbar.forEach {
                if (it is ImageView) {
                    clearView = it
                    return@forEach
                }
            }

            return clearView ?: throw AssertionError("Could not find clear view")
        }
    }

    infix fun View.assertIn(group: ViewGroup) {
        var found = false

        group.forEach {
            if (this == it) {
                println("Checking $this == $it")
                found = true
            }
        }

        if (!found) {
            throw AssertionError("View not found in ViewGroup")
        }
    }
}
