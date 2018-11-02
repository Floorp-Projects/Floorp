/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SuggestionViewHolderTest {
    @Test
    fun `DefaultViewHolder sets title and description`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(
            R.layout.mozac_browser_awesomebar_item_generic, null, false)

        val viewHolder = SuggestionViewHolder.DefaultSuggestionViewHolder(mock(), view)

        val suggestion = AwesomeBar.Suggestion(
            title = "Hello World",
            description = "https://www.mozilla.org")

        viewHolder.bind(suggestion)

        val titleView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_title)
        assertEquals("Hello World", titleView.text)

        val descriptionView = view.findViewById<TextView>(R.id.mozac_browser_awesomebar_description)
        assertEquals("https://www.mozilla.org", descriptionView.text)
    }

    @Test
    fun `Clicking on default suggestion view invokes callback`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(
            R.layout.mozac_browser_awesomebar_item_generic, null, false)

        val viewHolder = SuggestionViewHolder.DefaultSuggestionViewHolder(mock(), view)

        var callbackExecuted = false
        val suggestion = AwesomeBar.Suggestion(
            onSuggestionClicked = { callbackExecuted = true }
        )

        view.performClick()
        assertFalse(callbackExecuted)

        viewHolder.bind(suggestion)

        view.performClick()
        assertTrue(callbackExecuted)
    }

    @Test
    fun `ChipsSuggestionViewHolder adds views for chips`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(
            R.layout.mozac_browser_awesomebar_item_chips, null, false)

        val viewHolder = SuggestionViewHolder.ChipsSuggestionViewHolder(mock(), view)

        val suggestion = AwesomeBar.Suggestion(
            chips = listOf(
                AwesomeBar.Suggestion.Chip("Hello"),
                AwesomeBar.Suggestion.Chip("World"),
                AwesomeBar.Suggestion.Chip("Example")))

        val container = view.findViewById<ViewGroup>(R.id.mozac_browser_awesomebar_chips)

        assertEquals(0, container.childCount)

        viewHolder.bind(suggestion)

        assertEquals(3, container.childCount)

        assertEquals("Hello", (container.getChildAt(0) as TextView).text)
        assertEquals("World", (container.getChildAt(1) as TextView).text)
        assertEquals("Example", (container.getChildAt(2) as TextView).text)
    }

    @Test
    fun `Clicking on a chip invokes callback`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(
            R.layout.mozac_browser_awesomebar_item_chips, null, false)

        val viewHolder = SuggestionViewHolder.ChipsSuggestionViewHolder(mock(), view)

        var chipClicked: String? = null

        val suggestion = AwesomeBar.Suggestion(
            chips = listOf(
                AwesomeBar.Suggestion.Chip("Hello"),
                AwesomeBar.Suggestion.Chip("World"),
                AwesomeBar.Suggestion.Chip("Example")),
            onChipClicked = {
                chipClicked = it.title
            })

        viewHolder.bind(suggestion)

        val container = view.findViewById<ViewGroup>(R.id.mozac_browser_awesomebar_chips)

        container.getChildAt(0).performClick()
        assertEquals("Hello", chipClicked)

        container.getChildAt(1).performClick()
        assertEquals("World", chipClicked)

        container.getChildAt(2).performClick()
        assertEquals("Example", chipClicked)
    }
}
