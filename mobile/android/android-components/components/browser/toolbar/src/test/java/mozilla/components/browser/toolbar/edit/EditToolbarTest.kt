/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.view.KeyEvent
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class EditToolbarTest {
    private fun createEditToolbar(): Pair<BrowserToolbar, EditToolbar> {
        val toolbar: BrowserToolbar = mock()
        val displayToolbar = EditToolbar(
            testContext,
            toolbar,
            View.inflate(testContext, R.layout.mozac_browser_toolbar_edittoolbar, null),
        )
        return Pair(toolbar, displayToolbar)
    }

    @Test
    fun `entered text is forwarded to async autocomplete filter`() = runTest {
        val toolbar = BrowserToolbar(testContext)

        toolbar.edit.views.url.onAttachedToWindow()

        val latch = CountDownLatch(1)
        var invokedWithParams: List<Any?>? = null
        toolbar.setAutocompleteListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
            latch.countDown()
        }

        toolbar.edit.views.url.setText("Hello")

        // Autocomplete filter will be invoked on a worker thread.
        // Serialize here for the sake of tests.
        latch.await()

        assertEquals("Hello", invokedWithParams!![0])
        assertTrue(invokedWithParams!![1] is AutocompleteDelegate)
    }

    @Test
    fun `focus change is forwarded to listener`() {
        var listenerInvoked = false
        var value = false

        val toolbar = BrowserToolbar(testContext)
        toolbar.edit.setOnEditFocusChangeListener { hasFocus ->
            listenerInvoked = true
            value = hasFocus
        }

        // Switch to editing mode and focus view.
        toolbar.editMode()
        toolbar.edit.views.url.requestFocus()

        assertTrue(listenerInvoked)
        assertTrue(value)

        // Switch back to display mode
        listenerInvoked = false
        toolbar.displayMode()

        assertTrue(listenerInvoked)
        assertFalse(value)
    }

    @Test
    fun `entering text emits facts`() {
        CollectionProcessor.withFactCollection { facts ->
            val toolbar = BrowserToolbar(testContext)
            toolbar.edit.views.url.onAttachedToWindow()

            assertEquals(0, facts.size)

            toolbar.edit.views.url.setText("https://www.mozilla.org")
            toolbar.edit.views.url.dispatchKeyEvent(
                KeyEvent(
                    System.currentTimeMillis(),
                    System.currentTimeMillis(),
                    KeyEvent.ACTION_DOWN,
                    KeyEvent.KEYCODE_ENTER,
                    0,
                ),
            )

            assertEquals(2, facts.size)

            val factDetail = facts[0]
            assertEquals(Component.UI_AUTOCOMPLETE, factDetail.component)
            assertEquals(Action.IMPLEMENTATION_DETAIL, factDetail.action)
            assertEquals("onTextChanged", factDetail.item)
            assertEquals("InlineAutocompleteEditText", factDetail.value)

            val fact = facts[1]
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
    fun `entering text emits facts with autocomplete metadata`() {
        CollectionProcessor.withFactCollection { facts ->
            val toolbar = BrowserToolbar(testContext)
            toolbar.edit.views.url.onAttachedToWindow()

            assertEquals(0, facts.size)

            toolbar.edit.views.url.setText("https://www.mozilla.org")

            // Fake autocomplete
            toolbar.edit.views.url.autocompleteResult = InlineAutocompleteEditText.AutocompleteResult(
                text = "hello world",
                source = "test-source",
                totalItems = 100,
            )

            toolbar.edit.views.url.dispatchKeyEvent(
                KeyEvent(
                    System.currentTimeMillis(),
                    System.currentTimeMillis(),
                    KeyEvent.ACTION_DOWN,
                    KeyEvent.KEYCODE_ENTER,
                    0,
                ),
            )

            assertEquals(2, facts.size)

            val factDetail = facts[0]
            assertEquals(Component.UI_AUTOCOMPLETE, factDetail.component)
            assertEquals(Action.IMPLEMENTATION_DETAIL, factDetail.action)
            assertEquals("onTextChanged", factDetail.item)
            assertEquals("InlineAutocompleteEditText", factDetail.value)

            val factCommit = facts[1]
            assertEquals(Component.BROWSER_TOOLBAR, factCommit.component)
            assertEquals(Action.COMMIT, factCommit.action)
            assertEquals("toolbar", factCommit.item)
            assertNull(factCommit.value)

            val metadata = factCommit.metadata
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
    fun `clearView gone on init`() {
        val (_, editToolbar) = createEditToolbar()
        val clearView = editToolbar.views.clear
        assertTrue(clearView.visibility == View.GONE)
    }

    @Test
    fun `clearView visible on updateUrl`() {
        val (_, editToolbar) = createEditToolbar()
        val clearView = editToolbar.views.clear

        editToolbar.updateUrl("TestUrl", false)
        assertTrue(clearView.visibility == View.VISIBLE)
    }

    @Test
    fun `setIconClickListener sets a click listener on the icon view`() {
        val (_, editToolbar) = createEditToolbar()
        val iconView = editToolbar.views.icon
        assertFalse(iconView.hasOnClickListeners())
        editToolbar.setIconClickListener { /* noop */ }
        assertTrue(iconView.hasOnClickListeners())
    }

    @Test
    fun `clearView clears text in urlView`() {
        val (_, editToolbar) = createEditToolbar()
        val clearView = editToolbar.views.clear

        editToolbar.views.url.setText("https://www.mozilla.org")
        assertTrue(editToolbar.views.url.text.isNotBlank())

        assertNotNull(clearView)
        clearView.performClick()
        assertTrue(editToolbar.views.url.text.isBlank())
    }

    @Test
    fun `editSuggestion sets text in urlView`() {
        val (_, editToolbar) = createEditToolbar()
        val url = editToolbar.views.url

        url.setText("https://www.mozilla.org")
        assertEquals("https://www.mozilla.org", url.text.toString())

        var callbackCalled = false

        editToolbar.editListener = object : Toolbar.OnEditListener {
            override fun onTextChanged(text: String) {
                callbackCalled = true
            }
        }

        editToolbar.editSuggestion("firefox")

        assertEquals("firefox", url.text.toString())
        assertTrue(callbackCalled)
        assertEquals("firefox".length, url.selectionStart)
        assertTrue(url.hasFocus())
    }
}
