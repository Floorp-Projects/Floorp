/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.view

import android.widget.EditText
import androidx.appcompat.widget.AppCompatImageButton
import androidx.core.view.inputmethod.EditorInfoCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.feature.findinpage.R
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.shadows.ShadowLooper

@RunWith(AndroidJUnit4::class)
class FindInPageBarTest {

    @Test
    fun `Clicking close button invokes onClose method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(testContext)
        view.listener = listener

        view.findViewById<AppCompatImageButton>(R.id.find_in_page_close_btn)
            .performClick()

        verify(listener).onClose()
    }

    @Test
    fun `Clicking next button invokes onNextResult method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(testContext)
        view.listener = listener

        view.findViewById<EditText>(R.id.find_in_page_query_text).setText("Non empty query")
        view.findViewById<AppCompatImageButton>(R.id.find_in_page_next_btn)
            .performClick()

        verify(listener).onNextResult()
    }

    @Test
    fun `Clicking previous button invokes onPreviousResult method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(testContext)
        view.listener = listener

        view.findViewById<EditText>(R.id.find_in_page_query_text).setText("Non empty query")
        view.findViewById<AppCompatImageButton>(R.id.find_in_page_prev_btn)
            .performClick()

        verify(listener).onPreviousResult()
    }

    @Test
    fun `Entering text invokes onFindAll method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(testContext)
        view.listener = listener

        view.findViewById<EditText>(R.id.find_in_page_query_text)
            .setText("Hello World")

        verify(listener).onFindAll("Hello World")
    }

    @Test
    fun `Clearing text invokes onClearMatches method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(testContext)
        view.listener = listener

        view.findViewById<EditText>(R.id.find_in_page_query_text)
            .setText("")

        verify(listener).onClearMatches()
    }

    @Test
    fun `displayResult with matches will update views`() {
        val view = spy(FindInPageBar(testContext))

        view.displayResult(FindResultState(0, 100, false))

        val textCorrectValue = view.resultFormat.format(1, 100)
        val contentDesCorrectValue = view.accessibilityFormat.format(1, 100)

        assertEquals(textCorrectValue, view.resultsCountTextView.text)
        assertEquals(contentDesCorrectValue, view.resultsCountTextView.contentDescription)
        verify(view).announceForAccessibility(contentDesCorrectValue)
    }

    @Test
    fun `displayResult with no matches will update views`() {
        val view = spy(FindInPageBar(testContext))

        view.displayResult(FindResultState(0, 0, false))

        val textCorrectValue = view.resultFormat.format(0, 0)
        val contentDesCorrectValue = view.accessibilityFormat.format(0, 0)

        assertEquals(textCorrectValue, view.resultsCountTextView.text)
        assertEquals(contentDesCorrectValue, view.resultsCountTextView.contentDescription)
        verify(view).announceForAccessibility(contentDesCorrectValue)
    }

    @Test
    fun `private flag sets IME_FLAG_NO_PERSONALIZED_LEARNING on find in page bar`() {
        val findInPageBar = spy(FindInPageBar(testContext))
        val edit = findInPageBar.queryEditText

        // By default "private mode" is off.
        assertEquals(
            0,
            edit.imeOptions and
                EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
        )
        assertEquals(false, findInPageBar.private)

        // Turning on private mode sets flag
        findInPageBar.private = true
        assertNotEquals(
            0,
            edit.imeOptions and
                EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
        )
        assertTrue(findInPageBar.private)

        // Turning private mode off again - should remove flag
        findInPageBar.private = false
        assertEquals(
            0,
            edit.imeOptions and
                EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
        )
        assertEquals(false, findInPageBar.private)
    }

    @Test
    fun `clearing the focus of the find in page bar hides the keyboard`() {
        val findInPageBar = spy(FindInPageBar(testContext))

        // re-initialize the listener to use the spy
        findInPageBar.bindQueryEditText()

        // Focus the find in page bar first
        findInPageBar.focus()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks()

        // clearing the focus should hide the keyboard
        findInPageBar.clear()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks()
        verify(findInPageBar).hideKeyboard()
    }
}
