/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.layout

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.awesomebar.R
import mozilla.components.browser.awesomebar.widget.FlowLayout
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DefaultSuggestionViewHolderTest {

    @Test
    fun `DefaultViewHolder sets title and description`() {
        val view = LayoutInflater.from(testContext).inflate(
            R.layout.mozac_browser_awesomebar_item_generic, null, false)

        val awesomeBar = BrowserAwesomeBar(testContext)
        val viewHolder = DefaultSuggestionViewHolder.Default(awesomeBar, view)

        val suggestion = AwesomeBar.Suggestion(
            mock(),
            title = "Hello World",
            description = "https://www.mozilla.org")

        viewHolder.bind(suggestion) {
            // Do nothing
        }

        val titleView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_title)
        assertEquals("Hello World", titleView.text)

        val descriptionView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_description)
        assertEquals("https://www.mozilla.org", descriptionView.text)
        assertEquals(View.VISIBLE, descriptionView.visibility)
    }

    @Test
    fun `DefaultViewHolder without description hides description view`() {
        val view = LayoutInflater.from(testContext).inflate(
                R.layout.mozac_browser_awesomebar_item_generic, null, false)

        val awesomeBar = BrowserAwesomeBar(testContext)
        val viewHolder = DefaultSuggestionViewHolder.Default(awesomeBar, view)

        val suggestion = AwesomeBar.Suggestion(
                mock(),
                title = "Hello World")

        viewHolder.bind(suggestion) {
            // Do nothing
        }

        val titleView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_title)
        assertEquals("Hello World", titleView.text)

        val descriptionView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_description)
        assertEquals(View.GONE, descriptionView.visibility)
    }

    @Test
    fun `Clicking on default suggestion view invokes callback`() {
        val view = LayoutInflater.from(testContext).inflate(
            R.layout.mozac_browser_awesomebar_item_generic, null, false)

        val viewHolder = DefaultSuggestionViewHolder.Default(
            BrowserAwesomeBar(testContext), view)

        var callbackExecuted = false
        val suggestion = AwesomeBar.Suggestion(
            mock(),
            onSuggestionClicked = { callbackExecuted = true }
        )

        view.performClick()
        assertFalse(callbackExecuted)

        viewHolder.bind(suggestion) {
            // Do nothing
        }

        view.performClick()
        assertTrue(callbackExecuted)
    }

    @Test
    fun `ChipsSuggestionViewHolder adds views for chips`() {
        val view = LayoutInflater.from(testContext).inflate(
            R.layout.mozac_browser_awesomebar_item_chips, null, false)

        val viewHolder = DefaultSuggestionViewHolder.Chips(
            BrowserAwesomeBar(testContext), view)

        val suggestion = AwesomeBar.Suggestion(
            mock(),
            chips = listOf(
                AwesomeBar.Suggestion.Chip("Hello"),
                AwesomeBar.Suggestion.Chip("World"),
                AwesomeBar.Suggestion.Chip("Example")))

        val container = view.findViewById<ViewGroup>(R.id.mozac_browser_awesomebar_chips)

        assertEquals(0, container.childCount)

        viewHolder.bind(suggestion) {
            // Do nothing.
        }

        assertEquals(3, container.childCount)

        assertEquals("Hello", (container.getChildAt(0) as TextView).text)
        assertEquals("World", (container.getChildAt(1) as TextView).text)
        assertEquals("Example", (container.getChildAt(2) as TextView).text)
    }

    @Test
    fun `Clicking on a chip invokes callback`() {
        val view = LayoutInflater.from(testContext).inflate(
            R.layout.mozac_browser_awesomebar_item_chips, null, false)

        val viewHolder = DefaultSuggestionViewHolder.Chips(
            BrowserAwesomeBar(testContext), view)

        var chipClicked: String? = null

        val suggestion = AwesomeBar.Suggestion(
            mock(),
            chips = listOf(
                AwesomeBar.Suggestion.Chip("Hello"),
                AwesomeBar.Suggestion.Chip("World"),
                AwesomeBar.Suggestion.Chip("Example")),
            onChipClicked = {
                chipClicked = it.title
            })

        viewHolder.bind(suggestion) {
            // Do nothing.
        }

        val container = view.findViewById<ViewGroup>(R.id.mozac_browser_awesomebar_chips)

        container.getChildAt(0).performClick()
        assertEquals("Hello", chipClicked)

        container.getChildAt(1).performClick()
        assertEquals("World", chipClicked)

        container.getChildAt(2).performClick()
        assertEquals("Example", chipClicked)
    }

    @Test
    fun `FlowLayout for chips has spacing applied`() {
        val view = LayoutInflater.from(testContext).inflate(
            R.layout.mozac_browser_awesomebar_item_chips, null, false)

        val flowLayout = view.findViewById<FlowLayout>(R.id.mozac_browser_awesomebar_chips)

        assertEquals(0, flowLayout.spacing)

        val awesomeBar = BrowserAwesomeBar(testContext)
        DefaultSuggestionViewHolder.Chips(awesomeBar, view)

        assertEquals(2.dpToPx(testContext.resources.displayMetrics), flowLayout.spacing)
    }
}
