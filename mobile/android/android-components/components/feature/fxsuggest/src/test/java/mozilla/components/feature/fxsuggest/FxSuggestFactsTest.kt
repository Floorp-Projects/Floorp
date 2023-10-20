/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import mozilla.components.feature.fxsuggest.facts.FxSuggestFacts
import mozilla.components.feature.fxsuggest.facts.emitSuggestionClickedFact
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class FxSuggestFactsTest {

    @Test
    fun emitSuggestionClickedFactWithAmpInteractionInfo() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuggestionClickedFact(
                FxSuggestInteractionInfo.Amp(
                    blockId = 123,
                    advertiser = "mozilla",
                    reportingUrl = "https://example.com/reporting",
                    iabCategory = "22 - Shopping",
                    contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                ),
                positionInAwesomeBar = 0,
            )

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_FXSUGGEST, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_CLICKED, item)

                val clickInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
                assertEquals(clickInfo.blockId, 123)
                assertEquals(clickInfo.advertiser, "mozilla")
                assertEquals(clickInfo.reportingUrl, "https://example.com/reporting")
                assertEquals(clickInfo.iabCategory, "22 - Shopping")
                assertEquals(clickInfo.contextId, "c303282d-f2e6-46ca-a04a-35d3d873712d")
            }
        }
    }
}
