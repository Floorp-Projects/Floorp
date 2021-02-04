/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class AwesomeBarFactsTest {

    @Test
    fun `Emits facts for current state`() {
        CollectionProcessor.withFactCollection { facts ->

            emitBookmarkSuggestionClickedFact()

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.BOOKMARK_SUGGESTION_CLICKED, item)
            }

            emitClipboardSuggestionClickedFact()

            assertEquals(2, facts.size)
            facts[1].apply {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.CLIPBOARD_SUGGESTION_CLICKED, item)
            }

            emitHistorySuggestionClickedFact()

            assertEquals(3, facts.size)
            facts[2].apply {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED, item)
            }

            emitSearchActionClickedFact()
            assertEquals(4, facts.size)
            facts[3].apply {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.SEARCH_ACTION_CLICKED, item)
            }

            emitSearchSuggestionClickedFact()
            assertEquals(5, facts.size)
            facts[4].apply {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED, item)
            }

            emitOpenTabSuggestionClickedFact()
            assertEquals(6, facts.size)
            facts[5].apply {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.OPENED_TAB_SUGGESTION_CLICKED, item)
            }
        }
    }
}
