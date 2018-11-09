/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.content.Context
import android.widget.LinearLayout
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.lang.IllegalArgumentException

@RunWith(RobolectricTestRunner::class)
class SuggestionsAdapterTest {
    private val context: Context
        get() = RuntimeEnvironment.application

    @Test
    fun `addSuggestions() should add suggestions of provider`() {
        val adapter = SuggestionsAdapter(mock())

        val suggestions = listOf<AwesomeBar.Suggestion>(
            mock(), mock(), mock())

        assertEquals(0, adapter.itemCount)

        adapter.addSuggestions(mockProvider(), suggestions)

        assertEquals(3, adapter.itemCount)
        assertEquals(suggestions, adapter.suggestions)
    }

    @Test
    fun `clearSuggestions removes suggestions from adapter`() {
        val adapter = SuggestionsAdapter(mock())

        adapter.addSuggestions(mockProvider(), listOf(
            mock(), mock(), mock()))

        assertEquals(3, adapter.itemCount)

        adapter.clearSuggestions()

        assertEquals(0, adapter.itemCount)
    }

    @Test
    fun `clearSuggestions does not remove suggestions if provider has set shouldClearSuggestions to false`() {
        val adapter = SuggestionsAdapter(mock())

        adapter.addSuggestions(mockProvider(shouldClearSuggestions = false), listOf(
            mock(), mock(), mock()))

        assertEquals(3, adapter.itemCount)

        adapter.clearSuggestions()

        assertEquals(3, adapter.itemCount)

        adapter.addSuggestions(mockProvider(shouldClearSuggestions = true), listOf(
            mock(), mock(), mock(), mock()
        ))

        assertEquals(7, adapter.itemCount)

        adapter.clearSuggestions()

        assertEquals(3, adapter.itemCount)
    }

    @Test
    fun `Suggestions are getting ordered by weight descending`() {
        val adapter = SuggestionsAdapter(mock())

        adapter.addSuggestions(mockProvider(), listOf(
            AwesomeBar.Suggestion(title = "Hello", score = 10),
            AwesomeBar.Suggestion(title = "World", score = 2),
            AwesomeBar.Suggestion(title = "How", score = 7),
            AwesomeBar.Suggestion(title = "is", score = 12),
            AwesomeBar.Suggestion(title = "the", score = 0),
            AwesomeBar.Suggestion(title = "weather", score = -2),
            AwesomeBar.Suggestion(title = "tomorrow", score = 1000)))

        assertEquals(7, adapter.itemCount)

        val suggestions = adapter.suggestions
        assertEquals(7, suggestions.size)

        assertEquals("tomorrow", suggestions[0].title)
        assertEquals("is", suggestions[1].title)
        assertEquals("Hello", suggestions[2].title)
        assertEquals("How", suggestions[3].title)
        assertEquals("World", suggestions[4].title)
        assertEquals("the", suggestions[5].title)
        assertEquals("weather", suggestions[6].title)
    }

    @Test
    fun `Adapter uses different view holder for suggestions with chips`() {
        val adapter = SuggestionsAdapter(mock())

        adapter.addSuggestions(mockProvider(), listOf(
            AwesomeBar.Suggestion(title = "Test"),
            AwesomeBar.Suggestion(title = "World", chips = listOf(
                AwesomeBar.Suggestion.Chip("Chip1"),
                AwesomeBar.Suggestion.Chip("Chip2")))))

        assertEquals(2, adapter.itemCount)

        assertEquals(SuggestionViewHolder.DefaultSuggestionViewHolder.LAYOUT_ID,
            adapter.getItemViewType(0))

        assertEquals(SuggestionViewHolder.ChipsSuggestionViewHolder.LAYOUT_ID,
            adapter.getItemViewType(1))
    }

    @Test
    fun `onCreateViewHolder() will create view holder matching layout id`() {
        val adapter = SuggestionsAdapter(BrowserAwesomeBar(context))

        val parent = LinearLayout(context)

        assertTrue(adapter.createViewHolder(parent, SuggestionViewHolder.DefaultSuggestionViewHolder.LAYOUT_ID)
            is SuggestionViewHolder.DefaultSuggestionViewHolder)

        assertTrue(adapter.createViewHolder(parent, SuggestionViewHolder.ChipsSuggestionViewHolder.LAYOUT_ID)
            is SuggestionViewHolder.ChipsSuggestionViewHolder)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `onCreateViewHolder() will throw for unknown id`() {
        val adapter = SuggestionsAdapter(mock())

        val parent = LinearLayout(context)

        adapter.onCreateViewHolder(parent, 0)
    }

    @Test
    fun `onBindViewHolder() calls bind on the view holder`() {
        val adapter = spy(SuggestionsAdapter(mock()))

        val suggestion: AwesomeBar.Suggestion = mock()

        val viewHolder: SuggestionViewHolder = mock()
        doReturn(viewHolder).`when`(adapter).onCreateViewHolder(any(), anyInt())

        adapter.addSuggestions(mockProvider(), listOf(suggestion))

        adapter.onBindViewHolder(viewHolder, 0)

        verify(viewHolder).bind(suggestion)
    }

    private fun mockProvider(
        shouldClearSuggestions: Boolean = true
    ): AwesomeBar.SuggestionProvider {
        val provider: AwesomeBar.SuggestionProvider = mock()
        doReturn(shouldClearSuggestions).`when`(provider).shouldClearSuggestions
        return provider
    }
}
