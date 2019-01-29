/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.support.v7.widget.AppCompatImageButton
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.Session.FindResult
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.findinpage.FindInPageView
import mozilla.components.feature.findinpage.R
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FindInPageViewTest {
    private lateinit var context: Context
    private lateinit var mockEngineSession: EngineSession
    private lateinit var findInPageView: FindInPageView

    @Before
    fun setup() {
        context = ApplicationProvider.getApplicationContext()
        findInPageView = FindInPageView(context)
        mockEngineSession = mock()
    }

    @Test
    fun `Calling onCloseButtonClicked must notify the onCloseButtonPress listener`() {
        var closeListenerWasCalled = false
        findInPageView.engineSession = mockEngineSession

        findInPageView.onCloseButtonPressListener = {
            closeListenerWasCalled = true
        }

        findInPageView.onCloseButtonClicked()
        assertTrue(closeListenerWasCalled)
    }

    @Test
    fun `When type new text the widget must forward its calls to engineSession findAll`() {
        val newText = "N"
        findInPageView.engineSession = mockEngineSession

        findInPageView.queryEditText.setText(newText)

        verify(mockEngineSession).findAll(newText)
    }

    @Test
    fun `When type an empty string the widget must be clear up`() {
        val newText = " "
        findInPageView.engineSession = mockEngineSession

        findInPageView.queryEditText.setText(newText)

        assertTrue(findInPageView.resultsCountTextView.text.isEmpty())
        verify(mockEngineSession).clearFindMatches()
    }

    @Test
    fun `Calling onPreviousButtonClicked must forward its calls to engineSession findNext`() {
        findInPageView.engineSession = mockEngineSession

        val button = findInPageView.findViewById<AppCompatImageButton>(R.id.find_in_page_prev_btn)
        button.performClick()

        verify(mockEngineSession).findNext(false)
    }

    @Test
    fun `Calling onNextButtonClicked must forward its calls to engineSession findNext`() {
        findInPageView.engineSession = mockEngineSession

        val button = findInPageView.findViewById<AppCompatImageButton>(R.id.find_in_page_next_btn)
        button.performClick()

        verify(mockEngineSession).findNext(true)
    }

    @Test
    fun `Calling onCloseButtonClicked must reset the state of the widget`() {
        findInPageView.engineSession = mockEngineSession

        assertFalse(findInPageView.isActive())

        findInPageView.show()

        findInPageView.onFindResultReceived(FindResult(0, 1, false))

        assertTrue(findInPageView.queryEditText.isFocused)
        assertNotNull(findInPageView.queryEditText.text)
        assertNotNull(findInPageView.resultsCountTextView.text)

        val button = findInPageView.findViewById<AppCompatImageButton>(R.id.find_in_page_close_btn)
        button.performClick()

        assertFalse(findInPageView.isActive())
        assertTrue(findInPageView.queryEditText.text.isEmpty())
        assertTrue(findInPageView.resultsCountTextView.text.isEmpty())
    }

    @Test(expected = Exception::class)
    fun `Calling show without first initializing the engineSession will throw an exception`() {
        findInPageView.show()
    }

    @Test
    fun `Calling onFindResultReceived with a FindResult that has matches must populate the widget`() {
        findInPageView.engineSession = mockEngineSession

        findInPageView.onFindResultReceived(FindResult(0, 1, false))

        val textCorrectValue = findInPageView.resultFormat.format(1, 1)
        val contentDesCorrectValue = findInPageView.accessibilityFormat.format(1, 1)

        assertEquals(findInPageView.resultsCountTextView.text, textCorrectValue)
        assertEquals(findInPageView.resultsCountTextView.contentDescription, contentDesCorrectValue)
    }

    @Test
    fun `Calling onFindResultReceived with a FindResult that has zero matches must not populate the widget`() {
        findInPageView.engineSession = mockEngineSession

        findInPageView.onFindResultReceived(FindResult(0, 0, false))

        assertEquals(findInPageView.resultsCountTextView.text, "")
        assertEquals(findInPageView.resultsCountTextView.contentDescription, "")
    }
}