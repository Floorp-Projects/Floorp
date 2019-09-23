/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.graphics.Rect
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.core.view.contains
import androidx.core.view.forEach
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
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
import java.util.concurrent.CountDownLatch

@RunWith(AndroidJUnit4::class)
class EditToolbarTest {

    @Test
    fun `entered text is forwarded to async autocomplete filter`() {
        val toolbar = BrowserToolbar(testContext)
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

        val toolbar = BrowserToolbar(testContext)
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
            val toolbar = BrowserToolbar(testContext)
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
            val toolbar = BrowserToolbar(testContext)
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
    fun `clearView gone on init`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val editToolbar = EditToolbar(testContext, toolbar)
        val clearView = extractClearView(editToolbar)
        assertTrue(clearView.visibility == View.GONE)
    }

    @Test
    fun `clearView clears text in urlView`() {
        val toolbar = mock(BrowserToolbar::class.java)
        val editToolbar = EditToolbar(testContext, toolbar)
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
        val editToolbar = EditToolbar(testContext, toolbar)
        val clearView = extractClearView(editToolbar)

        editToolbar.updateClearViewVisibility("")
        assertTrue(clearView.visibility == View.GONE)

        editToolbar.updateClearViewVisibility("https://www.mozilla.org")
        assertTrue(clearView.visibility == View.VISIBLE)
    }

    @Test
    fun `clearView changes image color filter on update`() {
        val toolbar = BrowserToolbar(testContext)
        val editToolbar = toolbar.editToolbar
        editToolbar.clearViewColor = R.color.photonBlue40

        assertEquals(R.color.photonBlue40, editToolbar.clearViewColor)
    }

    @Test
    fun `WHEN edit actions are added THEN views are measured correctly`() {
        val toolbar: BrowserToolbar = mock()
        val editToolbar = EditToolbar(testContext, toolbar)

        editToolbar.addEditAction(BrowserToolbar.Button(mock(), "Microphone") {})
        editToolbar.addEditAction(BrowserToolbar.Button(mock(), "QR code scanner") {})

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        editToolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, editToolbar.measuredWidth)
        assertEquals(56, editToolbar.measuredHeight)
        assertEquals(0, editToolbar.paddingLeft)
        assertEquals(0, editToolbar.paddingRight)
        assertEquals(0, editToolbar.paddingTop)
        assertEquals(0, editToolbar.paddingBottom)

        val clearView = extractClearView(editToolbar)

        assertEquals(56, clearView.measuredWidth)
        assertEquals(56, clearView.measuredHeight)
        assertEquals(16, clearView.paddingLeft)
        assertEquals(16, clearView.paddingRight)
        assertEquals(16, clearView.paddingTop)
        assertEquals(16, clearView.paddingBottom)

        val microphoneView = extractActionView(editToolbar, "Microphone")

        assertEquals(56, microphoneView.measuredWidth)
        assertEquals(56, microphoneView.measuredHeight)
        assertEquals(16, microphoneView.paddingLeft)
        assertEquals(16, microphoneView.paddingRight)
        assertEquals(16, microphoneView.paddingTop)
        assertEquals(16, microphoneView.paddingBottom)

        val qrView = extractActionView(editToolbar, "QR code scanner")

        assertEquals(56, qrView.measuredWidth)
        assertEquals(56, qrView.measuredHeight)
        assertEquals(0, qrView.paddingLeft)
        assertEquals(16, qrView.paddingRight)
        assertEquals(16, qrView.paddingTop)
        assertEquals(16, qrView.paddingBottom)

        val urlView = extractUrlView(editToolbar)

        assertEquals(856, urlView.measuredWidth)
        assertEquals(56, urlView.measuredHeight)
        assertEquals(8, urlView.paddingLeft)
        assertEquals(8, urlView.paddingRight)
        assertEquals(0, urlView.paddingTop)
        assertEquals(0, urlView.paddingBottom)
    }

    @Test
    fun `WHEN edit actions are added THEN views are layout correctly`() {
        val toolbar: BrowserToolbar = mock()
        val editToolbar = EditToolbar(testContext, toolbar)

        editToolbar.addEditAction(BrowserToolbar.Button(mock(), "Microphone") {})
        editToolbar.addEditAction(BrowserToolbar.Button(mock(), "QR code scanner") {})

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        editToolbar.measure(widthSpec, heightSpec)
        editToolbar.layout(0, 0, 1024, 56)

        val urlView = extractUrlView(editToolbar)
        val microphoneView = extractActionView(editToolbar, "Microphone")
        val qrView = extractActionView(editToolbar, "QR code scanner")
        val clearView = extractClearView(editToolbar)

        val toolbarRect = Rect(editToolbar.left, editToolbar.top, editToolbar.right, editToolbar.bottom)

        val urlRect = Rect(urlView.left, urlView.top, urlView.right, urlView.bottom)
        val microphoneRect = Rect(microphoneView.left, microphoneView.top, microphoneView.right, microphoneView.bottom)
        val qrRect = Rect(qrView.left, qrView.top, qrView.right, qrView.bottom)
        val clearRect = Rect(clearView.left, clearView.top, clearView.right, clearView.bottom)

        assertTrue(toolbarRect.contains(urlRect))
        assertTrue(toolbarRect.contains(microphoneRect))
        assertTrue(toolbarRect.contains(qrRect))
        assertTrue(toolbarRect.contains(clearRect))

        assertEquals(
            Rect(0, 0, 856, 56),
            urlRect)

        assertEquals(
            Rect(856, 0, 912, 56),
            microphoneRect)

        assertEquals(
            Rect(912, 0, 968, 56),
            qrRect)

        assertEquals(
            Rect(968, 0, 1024, 56),
            clearRect)
    }

    companion object {
        private fun extractClearView(editToolbar: EditToolbar): ImageView =
            extractView(editToolbar) {
                it?.id == R.id.mozac_browser_toolbar_clear_view
            } ?: throw AssertionError("Could not find clear view")

        private fun extractUrlView(editToolbar: EditToolbar): View =
            extractView(editToolbar) ?: throw AssertionError("Could not find URL input view")

        private fun extractActionView(
            editToolbar: EditToolbar,
            contentDescription: String
        ): View = extractView(editToolbar) {
            it?.contentDescription == contentDescription
        } ?: throw AssertionError("Could not find action view: $contentDescription")

        private inline fun <reified T> extractView(
            editToolbar: EditToolbar,
            otherCondition: (T) -> Boolean = { true }
        ): T? {
            editToolbar.forEach {
                if (it is T && otherCondition(it)) {
                    return it
                }
            }
            return null
        }
    }

    infix fun View.assertIn(group: ViewGroup) {
        if (!group.contains(this)) {
            throw AssertionError("View not found in ViewGroup")
        }
    }
}
