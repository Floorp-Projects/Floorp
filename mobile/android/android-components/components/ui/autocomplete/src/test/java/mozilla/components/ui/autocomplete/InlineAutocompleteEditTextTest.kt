/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.autocomplete

import android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
import android.view.KeyEvent
import android.view.ViewParent
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText.AutocompleteResult
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText.Companion.AUTOCOMPLETE_SPAN
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric.buildAttributeSet

@RunWith(AndroidJUnit4::class)
class InlineAutocompleteEditTextTest {

    private val attributes = buildAttributeSet().build()

    @Test
    fun autoCompleteResult() {
        val result = AutocompleteResult("testText", "testSource", 1)
        assertEquals("testText", result.text)
        assertEquals("testSource", result.source)
        assertEquals(1, result.totalItems)
    }

    @Test
    fun getNonAutocompleteText() {
        val et = InlineAutocompleteEditText(testContext)
        et.setText("Test")
        assertEquals("Test", et.nonAutocompleteText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 2, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("Te", et.nonAutocompleteText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 0, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("", et.nonAutocompleteText)
    }

    @Test
    fun getOriginalText() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("Test")
        assertEquals("Test", et.originalText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 2, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("Test", et.originalText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 0, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("Test", et.originalText)
    }

    @Test
    fun onFocusChange() {
        val et = InlineAutocompleteEditText(testContext, attributes, R.attr.editTextStyle)
        val searchStates = mutableListOf<Boolean>()

        et.setOnSearchStateChangeListener { b: Boolean -> searchStates.add(searchStates.size, b) }
        et.onFocusChanged(false, 0, null)

        et.setText("text")
        et.text.setSpan(AUTOCOMPLETE_SPAN, 0, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        et.onFocusChanged(false, 0, null)
        assertTrue(et.text.isEmpty())

        et.setText("text")
        et.text.setSpan(AUTOCOMPLETE_SPAN, 0, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        et.onFocusChanged(true, 0, null)
        assertFalse(et.text.isEmpty())
        assertEquals(listOf(false, true, true), searchStates)
    }

    @Test
    fun sendAccessibilityEventUnchecked() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        doReturn(false).`when`(et).isShown
        doReturn(mock(ViewParent::class.java)).`when`(et).parent

        val event = AccessibilityEvent.obtain()
        event.eventType = AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED
        et.sendAccessibilityEventUnchecked(event)

        verify(et).onInitializeAccessibilityEvent(event)
        verify(et).dispatchPopulateAccessibilityEvent(event)
        verify(et.parent).requestSendAccessibilityEvent(et, event)
    }

    @Test
    fun onAutocompleteSetsEmptyResult() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))

        doReturn(false).`when`(et).isEnabled
        et.applyAutocompleteResult(AutocompleteResult("text", "source", 1))
        assertNull(et.autocompleteResult)
    }

    @Test
    fun onAutocompleteDiscardsStaleResult() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        doReturn(true).`when`(et).isEnabled
        et.setText("text")

        et.applyAutocompleteResult(AutocompleteResult("stale result", "source", 1))
        assertEquals("text", et.text.toString())

        et.text.setSpan(AUTOCOMPLETE_SPAN, 1, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        et.applyAutocompleteResult(AutocompleteResult("stale result", "source", 1))
        assertEquals("text", et.text.toString())
    }

    @Test
    fun onAutocompleteReplacesExistingAutocompleteText() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.text.setSpan(AUTOCOMPLETE_SPAN, 1, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())
    }

    @Test
    fun onAutocompleteAppendsExistingText() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())
    }

    @Test
    fun onAutocompleteSetsSpan() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))

        assertEquals(4, et.text.getSpanStart(AUTOCOMPLETE_SPAN))
        assertEquals(14, et.text.getSpanEnd(AUTOCOMPLETE_SPAN))
        assertEquals(SPAN_EXCLUSIVE_EXCLUSIVE, et.text.getSpanFlags(AUTOCOMPLETE_SPAN))
    }

    @Test
    fun onKeyPreImeListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnKeyPreImeListener { p1, p2, p3 ->
            invokedWithParams = listOf(p1, p2, p3)
            true
        }
        val event = mock(KeyEvent::class.java)
        et.onKeyPreIme(1, event)
        assertEquals(listOf(et, 1, event), invokedWithParams)
    }

    @Test
    fun onSelectionChangedListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnSelectionChangedListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
        }
        et.onSelectionChanged(0, 1)
        assertEquals(listOf(0, 1), invokedWithParams)
    }

    @Test
    fun onSelectionChangedCommitsResult() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.onAttachedToWindow()

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals(4, et.text.getSpanStart(AUTOCOMPLETE_SPAN))

        et.onSelectionChanged(4, 14)
        assertEquals(-1, et.text.getSpanStart(AUTOCOMPLETE_SPAN))
    }

    @Test
    fun onWindowFocusChangedListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnWindowsFocusChangeListener { p1 ->
            invokedWithParams = listOf(p1)
        }
        et.onWindowFocusChanged(true)
        assertEquals(listOf(true), invokedWithParams)
    }

    @Test
    fun onCommitListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        var invoked = false
        et.setOnCommitListener { invoked = true }
        et.onAttachedToWindow()

        et.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER))
        assertTrue(invoked)
    }

    @Test
    fun onTextChangeListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnTextChangeListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
        }
        et.onAttachedToWindow()

        et.setText("text")
        assertEquals(listOf("text", "text"), invokedWithParams)
    }

    @Test
    fun onSearchStateChangeListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.onAttachedToWindow()

        var invokedWithParams: List<Any>? = null
        et.setOnSearchStateChangeListener { p1 ->
            invokedWithParams = listOf(p1)
        }

        et.setText("")
        assertEquals(listOf(false), invokedWithParams)

        et.setText("text")
        assertEquals(listOf(true), invokedWithParams)
    }

    @Test
    fun onFilterListenerInvocation() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.onAttachedToWindow()

        var lastInvokedWithText: String? = null
        var invokedCounter = 0
        et.setOnFilterListener { p1 ->
            lastInvokedWithText = p1
            invokedCounter++
        }

        // Already have an autocomplete result, and setting a text to the same value as the result.
        et.applyAutocompleteResult(AutocompleteResult("text", "source", 1))
        et.setText("text")
        // Autocomplete filter shouldn't have been called, because we already have a matching result.
        assertEquals(0, invokedCounter)

        et.setText("text")
        assertEquals(1, invokedCounter)
        assertEquals("text", lastInvokedWithText)

        // Test backspace. We don't expect autocomplete to have been called.
        et.setText("tex")
        assertEquals(1, invokedCounter)

        // Presence of a space is counted as a 'search query', we don't autocomplete those.
        et.setText("search term")
        assertEquals(1, invokedCounter)

        // Empty text isn't autocompleted either.
        et.setText("")
        assertEquals(1, invokedCounter)
    }

    @Test
    fun onCreateInputConnection() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        val icw = et.onCreateInputConnection(mock(EditorInfo::class.java))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        icw?.deleteSurroundingText(0, 1)
        assertNull(et.autocompleteResult)
        assertEquals("text", et.text.toString())

        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        BaseInputConnection.setComposingSpans(et.text)
        icw?.commitText("text", 4)
        assertNull(et.autocompleteResult)
        assertEquals("text", et.text.toString())

        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        BaseInputConnection.setComposingSpans(et.text)
        icw?.setComposingText("text", 4)
        assertNull(et.autocompleteResult)
        assertEquals("text", et.text.toString())
    }

    @Test
    fun removeAutocompleteOnComposing() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        val ic = et.onCreateInputConnection(mock(EditorInfo::class.java))

        ic?.setComposingText("text", 1)
        assertEquals("text", et.text.toString())

        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        // Simulating a backspace which should remove the autocomplete and leave original text
        ic?.setComposingText("tex", 1)
        assertEquals("text", et.text.toString())

        // Verify that we finished composing
        assertEquals(-1, BaseInputConnection.getComposingSpanStart(et.text))
        assertEquals(-1, BaseInputConnection.getComposingSpanEnd(et.text))
    }
}
