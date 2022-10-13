/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.autocomplete

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
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
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Robolectric.buildAttributeSet
import org.robolectric.annotation.Config

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

        // Autocomplete for the first letter
        et.setText("t")
        assertEquals(2, invokedCounter)
        et.applyAutocompleteResult(AutocompleteResult("text", "source", 1))

        // Autocomplete should be called for the next letter that doesn't match the result
        et.setText("ta")
        assertEquals(3, invokedCounter)
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

    @Test
    fun `GIVEN empty edit field WHEN text 'g' added THEN autocomplete to google`() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("")
        et.onAttachedToWindow()

        et.autocompleteResult = AutocompleteResult(
            text = "google.com",
            source = "test-source",
            totalItems = 100,
        )

        et.setText("g")
        assertEquals("google.com", "${et.text}")
    }

    @Test
    fun `GIVEN empty edit field WHEN text 'g ' added THEN don't autocomplete to google`() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("")
        et.onAttachedToWindow()

        et.autocompleteResult = AutocompleteResult(
            text = "google.com",
            source = "test-source",
            totalItems = 100,
        )

        et.setText("g ")
        assertEquals("g ", "${et.text}")
    }

    @Test
    fun `GIVEN field with 'google' WHEN backspacing THEN doesn't autocomplete`() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("google")
        et.onAttachedToWindow()

        et.autocompleteResult = AutocompleteResult(
            text = "google.com",
            source = "test-source",
            totalItems = 100,
        )

        et.setText("googl")
        assertEquals("googl", "${et.text}")
    }

    @Test
    fun `GIVEN field with selected text WHEN text 'g' added THEN autocomplete to google`() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("testestest")
        et.selectAll()
        et.onAttachedToWindow()
        et.autocompleteResult = AutocompleteResult(
            text = "google.com",
            source = "test-source",
            totalItems = 100,
        )

        et.setText("g")
        assertEquals("google.com", "${et.text}")
    }

    @Test
    fun `GIVEN field with selected text 'google ' WHEN text 'g' added THEN autocomplete to google`() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("https://www.google.com/")
        et.selectAll()
        et.onAttachedToWindow()
        et.autocompleteResult = AutocompleteResult(
            text = "google.com",
            source = "test-source",
            totalItems = 100,
        )

        et.setText("g")
        assertEquals("google.com", "${et.text}")
    }

    @Test
    fun `WHEN setting text THEN isEnabled is never modified`() {
        val et = spy(InlineAutocompleteEditText(testContext, attributes))
        et.setText("", shouldAutoComplete = false)
        // assigning here so it verifies the setter, not the getter
        verify(et, never()).isEnabled = true
    }

    @Test
    fun `WHEN onTextContextMenuItem is called for options other than paste THEN we should not paste() and just call super`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))

        editText.onTextContextMenuItem(android.R.id.copy)
        editText.onTextContextMenuItem(android.R.id.shareText)
        editText.onTextContextMenuItem(android.R.id.cut)
        editText.onTextContextMenuItem(android.R.id.selectAll)

        verify(editText, never()).paste(anyInt(), anyInt(), anyBoolean())
        verify(editText, times(4)).callOnTextContextMenuItemSuper(anyInt())
    }

    @Test
    fun `WHEN onTextContextMenuItem is called for paste THEN we should paste() and not call super`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))

        editText.onTextContextMenuItem(android.R.id.paste)

        verify(editText).paste(anyInt(), anyInt(), anyBoolean())
        verify(editText, never()).callOnTextContextMenuItemSuper(anyInt())
    }

    @Test
    fun `WHEN onTextContextMenuItem is called for pasteAsPlainText THEN we should paste() and not call super`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))

        editText.onTextContextMenuItem(android.R.id.pasteAsPlainText)

        verify(editText).paste(anyInt(), anyInt(), anyBoolean())
        verify(editText, never()).callOnTextContextMenuItemSuper(anyInt())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP, Build.VERSION_CODES.LOLLIPOP_MR1])
    fun `GIVEN an Android L device, WHEN onTextContextMenuItem is called for paste THEN we should paste() with formatting`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))

        editText.onTextContextMenuItem(android.R.id.paste)

        verify(editText).paste(anyInt(), anyInt(), eq(true))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M, Build.VERSION_CODES.N, Build.VERSION_CODES.O, Build.VERSION_CODES.P])
    fun `GIVEN an Android M device, WHEN onTextContextMenuItem is called for paste THEN we should paste() without formatting`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))

        editText.onTextContextMenuItem(android.R.id.paste)

        verify(editText).paste(anyInt(), anyInt(), eq(false))
    }

    @Test
    fun `GIVEN no previous text WHEN paste is selected THEN paste() should be called with 0,0`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))

        editText.onTextContextMenuItem(android.R.id.paste)

        verify(editText).paste(eq(0), eq(0), eq(false))
    }

    @Test
    fun `GIVEN 5 chars previous text WHEN paste is selected THEN paste() should be called with 0,5`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))
        editText.setText("chars")
        editText.selectAll()

        editText.onTextContextMenuItem(android.R.id.paste)

        verify(editText).paste(eq(0), eq(5), eq(false))
    }

    @Test
    fun `WHEN paste() is called with new text THEN we will display the new text`() {
        val editText = spy(InlineAutocompleteEditText(testContext, attributes))
        (testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager).apply {
            setPrimaryClip(ClipData.newPlainText("Test", "test"))
        }

        assertEquals("", editText.text.toString())

        editText.paste(0, 0, false)

        assertEquals("test", editText.text.toString())
    }

    fun `WHEN committing autocomplete THEN textChangedListener is invoked`() {
        val et = InlineAutocompleteEditText(testContext, attributes)
        et.setText("")

        et.onAttachedToWindow()
        et.autocompleteResult = AutocompleteResult(
            text = "google.com",
            source = "test-source",
            totalItems = 100,
        )
        et.setText("g")
        var callbackInvoked = false
        et.setOnTextChangeListener { _, _ ->
            callbackInvoked = true
        }
        et.setSelection(3)
        assertTrue(callbackInvoked)
    }
}
