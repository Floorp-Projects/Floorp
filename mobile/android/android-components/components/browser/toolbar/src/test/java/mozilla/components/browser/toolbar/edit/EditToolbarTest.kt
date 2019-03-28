/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.content.Context
import android.graphics.Rect
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
import mozilla.components.support.test.mock
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

    @Test
    fun `WHEN edit actions are added THEN views are measured correctly`() {
        val toolbar: BrowserToolbar = mock()
        val editToolbar = EditToolbar(context, toolbar)

        editToolbar.addEditAction(BrowserToolbar.Button(mock(), "Microphone") {})
        editToolbar.addEditAction(BrowserToolbar.Button(mock(), "QR code scanner") {})

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        editToolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, editToolbar.measuredWidth)
        assertEquals(56, editToolbar.measuredHeight)

        val clearView = extractClearView(editToolbar)

        assertEquals(56, clearView.measuredWidth)
        assertEquals(56, clearView.measuredHeight)

        val microphoneView = extractActionView(editToolbar, "Microphone")

        assertEquals(56, microphoneView.measuredWidth)
        assertEquals(56, microphoneView.measuredHeight)

        val qrView = extractActionView(editToolbar, "QR code scanner")

        assertEquals(56, qrView.measuredWidth)
        assertEquals(56, qrView.measuredHeight)

        val urlView = extractUrlView(editToolbar)

        assertEquals(856, urlView.measuredWidth)
        assertEquals(56, urlView.measuredHeight)
    }

    @Test
    fun `WHEN edit actions are added THEN views are layout correctly`() {
        val toolbar: BrowserToolbar = mock()
        val editToolbar = EditToolbar(context, toolbar)

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
        private fun extractClearView(editToolbar: EditToolbar): ImageView {
            var clearView: ImageView? = null

            editToolbar.forEach {
                if (it is ImageView && it.id == R.id.mozac_browser_toolbar_clear_view) {
                    clearView = it
                    return@forEach
                }
            }

            return clearView ?: throw AssertionError("Could not find clear view")
        }

        private fun extractUrlView(editToolbar: EditToolbar): View {
            var urlView: InlineAutocompleteEditText? = null

            editToolbar.forEach { view ->
                if (view is InlineAutocompleteEditText) {
                    urlView = view
                    return@forEach
                }
            }

            return urlView ?: throw java.lang.AssertionError("Could not find URL input view")
        }

        private fun extractActionView(
            editToolbar: EditToolbar,
            contentDescription: String
        ): View {
            var actionView: View? = null

            editToolbar.forEach { view ->
                if (view.contentDescription == contentDescription) {
                    actionView = view
                    return@forEach
                }
            }

            return actionView ?: throw java.lang.AssertionError("Could not find action view: $contentDescription")
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
