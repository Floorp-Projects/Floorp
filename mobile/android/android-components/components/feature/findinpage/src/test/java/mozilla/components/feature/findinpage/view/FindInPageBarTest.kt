/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.view

import android.content.Context
import android.support.v7.widget.AppCompatImageButton
import android.widget.EditText
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.Session
import mozilla.components.feature.findinpage.R
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FindInPageBarTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `Clicking close button invokes onClose method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(context)
        view.listener = listener

        view.findViewById<AppCompatImageButton>(R.id.find_in_page_close_btn)
            .performClick()

        verify(listener).onClose()
    }

    @Test
    fun `Clicking next button invokes onNextResult method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(context)
        view.listener = listener

        view.findViewById<AppCompatImageButton>(R.id.find_in_page_next_btn)
            .performClick()

        verify(listener).onNextResult()
    }

    @Test
    fun `Clicking previous button invokes onPreviousResult method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(context)
        view.listener = listener

        view.findViewById<AppCompatImageButton>(R.id.find_in_page_prev_btn)
            .performClick()

        verify(listener).onPreviousResult()
    }

    @Test
    fun `Entering text invokes onFindAll method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(context)
        view.listener = listener

        view.findViewById<EditText>(R.id.find_in_page_query_text)
            .setText("Hello World")

        verify(listener).onFindAll("Hello World")
    }

    @Test
    fun `Clearing text invokes onClearMatches method of listener`() {
        val listener: FindInPageView.Listener = mock()

        val view = FindInPageBar(context)
        view.listener = listener

        view.findViewById<EditText>(R.id.find_in_page_query_text)
            .setText("")

        verify(listener).onClearMatches()
    }

    @Test
    fun `displayResult with matches will update views`() {
        val view = spy(FindInPageBar(context))

        view.displayResult(Session.FindResult(0, 100, false))

        val textCorrectValue = view.resultFormat.format(1, 100)
        val contentDesCorrectValue = view.accessibilityFormat.format(1, 100)

        assertEquals(textCorrectValue, view.resultsCountTextView.text)
        assertEquals(contentDesCorrectValue, view.resultsCountTextView.contentDescription)
        verify(view).announceForAccessibility(contentDesCorrectValue)
    }

    @Test
    fun `displayResult with no matches will clear views`() {
        val view = FindInPageBar(context)

        view.displayResult(Session.FindResult(0, 0, false))

        assertEquals("", view.resultsCountTextView.text)
        assertEquals("", view.resultsCountTextView.contentDescription)
    }
}
