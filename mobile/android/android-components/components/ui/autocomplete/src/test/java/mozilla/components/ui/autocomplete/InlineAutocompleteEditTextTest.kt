/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.autocomplete

import android.content.Context
import android.text.Spanned
import android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.ViewParent
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.doReturn
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

import mozilla.components.ui.autocomplete.InlineAutocompleteEditText.AutocompleteResult
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText.Companion.AUTOCOMPLETE_SPAN

@RunWith(RobolectricTestRunner::class)
@Config(constants = BuildConfig::class)
class InlineAutocompleteEditTextTest {
    private val context: Context = RuntimeEnvironment.application
    private val attributes: AttributeSet = mock(AttributeSet::class.java)

    @Test
    fun testAutoCompleteResult() {
        val empty = AutocompleteResult.emptyResult()
        assertTrue(empty.isEmpty)
        assertEquals(0, empty.length)
        assertFalse(empty.startsWith("test"))

        val nonEmpty = AutocompleteResult("testText", "testSource", 1, { it.toUpperCase() })
        assertFalse(nonEmpty.isEmpty)
        assertEquals(8, nonEmpty.length)
        assertTrue(nonEmpty.startsWith("test"))
        assertEquals("testSource", nonEmpty.source)
        assertEquals(1, nonEmpty.totalItems)
        assertEquals("TESTTEXT", nonEmpty.formattedText)
    }

    @Test
    fun testGetNonAutocompleteText() {
        val et = InlineAutocompleteEditText(context)
        et.setText("Test")
        assertEquals("Test", et.nonAutocompleteText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 2, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("Te", et.nonAutocompleteText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 0, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("", et.nonAutocompleteText)
    }

    @Test
    fun testGetOriginalText() {
        val et = InlineAutocompleteEditText(context, attributes)
        et.setText("Test")
        assertEquals("Test", et.originalText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 2, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("Test", et.originalText)

        et.text.setSpan(AUTOCOMPLETE_SPAN, 0, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        assertEquals("Test", et.originalText)
    }

    @Test
    fun testOnFocusChange() {
        val et = InlineAutocompleteEditText(context, attributes, R.attr.editTextStyle)
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
    fun testSendAccessibilityEventUnchecked() {
        val et = spy(InlineAutocompleteEditText(context, attributes))
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
    fun testOnAutocompleteSetsEmptyResult() {
        val et = spy(InlineAutocompleteEditText(context, attributes))

        doReturn(false).`when`(et).isEnabled
        et.applyAutocompleteResult(AutocompleteResult("text", "source", 1))
        assertEquals(AutocompleteResult.emptyResult(), et.autocompleteResult)

        doReturn(true).`when`(et).isEnabled
        et.applyAutocompleteResult(AutocompleteResult.emptyResult())
        assertEquals(AutocompleteResult.emptyResult(), et.autocompleteResult)
    }

    @Test
    fun testOnAutocompleteDiscardsStaleResult() {
        val et = spy(InlineAutocompleteEditText(context, attributes))
        doReturn(true).`when`(et).isEnabled
        et.setText("text")

        et.applyAutocompleteResult(AutocompleteResult("stale result", "source", 1))
        assertEquals("text", et.text.toString())

        et.text.setSpan(AUTOCOMPLETE_SPAN, 1, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        et.applyAutocompleteResult(AutocompleteResult("stale result", "source", 1))
        assertEquals("text", et.text.toString())
    }

    @Test
    fun testOnAutocompleteReplacesExistingAutocompleteText() {
        val et = spy(InlineAutocompleteEditText(context, attributes))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.text.setSpan(AUTOCOMPLETE_SPAN, 1, 3, SPAN_EXCLUSIVE_EXCLUSIVE)
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())
    }

    @Test
    fun testOnAutocompleteAppendsExistingText() {
        val et = spy(InlineAutocompleteEditText(context, attributes))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())
    }

    @Test
    fun testOnAutocompleteSetsSpan() {
        val et = spy(InlineAutocompleteEditText(context, attributes))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))

        assertEquals(4, et.text.getSpanStart(AUTOCOMPLETE_SPAN))
        assertEquals(14, et.text.getSpanEnd(AUTOCOMPLETE_SPAN))
        assertEquals(Spanned.SPAN_EXCLUSIVE_EXCLUSIVE, et.text.getSpanFlags(AUTOCOMPLETE_SPAN))
    }

    @Test
    fun testOnKeyPreImeListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
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
    fun testOnSelectionChangedListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnSelectionChangedListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
        }
        et.onSelectionChanged(0, 1)
        assertEquals(listOf(0, 1), invokedWithParams)
    }

    @Test
    fun testOnSelectionChangedCommitsResult() {
        val et = InlineAutocompleteEditText(context, attributes)
        et.onAttachedToWindow()

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals(4, et.text.getSpanStart(AUTOCOMPLETE_SPAN))

        et.onSelectionChanged(4, 14)
        assertEquals(-1, et.text.getSpanStart(AUTOCOMPLETE_SPAN))
    }

    @Test
    fun testOnWindowFocusChangedListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnWindowsFocusChangeListener { p1 ->
            invokedWithParams = listOf(p1)
        }
        et.onWindowFocusChanged(true)
        assertEquals(listOf(true), invokedWithParams)
    }

    @Test
    fun testOnCommitListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
        var invoked: Boolean = false
        et.setOnCommitListener { invoked = true }
        et.onAttachedToWindow()

        et.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER))
        assertTrue(invoked)
    }

    @Test
    fun testOnTextChangeListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
        var invokedWithParams: List<Any>? = null
        et.setOnTextChangeListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
        }
        et.onAttachedToWindow()

        et.setText("text")
        assertEquals(listOf("text", "text"), invokedWithParams)
    }

    @Test
    fun testOnSearchStateChangeListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
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
    fun testOnFilterListenerInvocation() {
        val et = InlineAutocompleteEditText(context, attributes)
        et.onAttachedToWindow()

        var invokedWithParams: List<Any?>? = null
        et.setOnFilterListener { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
        }

        // Text existing autocomplete result
        et.applyAutocompleteResult(AutocompleteResult("text", "source", 1))
        et.setText("text")
        assertEquals(listOf("text", null), invokedWithParams)

        et.setText("text")
        assertEquals(listOf("text", et), invokedWithParams)

        // Test backspace
        et.setText("tex")
        assertEquals(listOf("tex", null), invokedWithParams)

        et.setText("search term")
        assertEquals(listOf("search term", null), invokedWithParams)

        et.setText("")
        assertEquals(listOf("", null), invokedWithParams)
    }

    @Test
    fun testOnCreateInputConnection() {
        val et = spy(InlineAutocompleteEditText(context, attributes))
        val icw = et.onCreateInputConnection(mock(EditorInfo::class.java))
        doReturn(true).`when`(et).isEnabled

        et.setText("text")
        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        icw?.deleteSurroundingText(0, 1)
        assertEquals(AutocompleteResult.emptyResult(), et.autocompleteResult)
        assertEquals("text", et.text.toString())

        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        BaseInputConnection.setComposingSpans(et.text)
        icw?.commitText("text", 4)
        assertEquals(AutocompleteResult.emptyResult(), et.autocompleteResult)
        assertEquals("text", et.text.toString())

        et.applyAutocompleteResult(AutocompleteResult("text completed", "source", 1))
        assertEquals("text completed", et.text.toString())

        BaseInputConnection.setComposingSpans(et.text)
        icw?.setComposingText("text", 4)
        assertEquals(AutocompleteResult.emptyResult(), et.autocompleteResult)
        assertEquals("text", et.text.toString())
    }

    @Test
    fun testRemoveAutocompleteOnComposing() {
        val et = InlineAutocompleteEditText(context, attributes)
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