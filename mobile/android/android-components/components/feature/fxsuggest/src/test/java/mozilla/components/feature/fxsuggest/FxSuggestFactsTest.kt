/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import mozilla.components.feature.fxsuggest.facts.FxSuggestFacts
import mozilla.components.feature.fxsuggest.facts.emitSuggestionClickedFact
import mozilla.components.feature.fxsuggest.facts.emitSuggestionImpressedFact
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class FxSuggestFactsTest {

    @Test
    fun `GIVEN interaction information for an AMP suggestion WHEN emitting a click fact THEN 1 click fact is collected`() {
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

                assertEquals(
                    setOf(
                        FxSuggestFacts.MetadataKeys.INTERACTION_INFO,
                        FxSuggestFacts.MetadataKeys.POSITION,
                    ),
                    metadata?.keys,
                )

                val clickInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
                assertEquals(clickInfo.blockId, 123)
                assertEquals(clickInfo.advertiser, "mozilla")
                assertEquals(clickInfo.reportingUrl, "https://example.com/reporting")
                assertEquals(clickInfo.iabCategory, "22 - Shopping")
                assertEquals(clickInfo.contextId, "c303282d-f2e6-46ca-a04a-35d3d873712d")

                val positionInAwesomebar = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
                assertEquals(0, positionInAwesomebar)
            }
        }
    }

    @Test
    fun `GIVEN interaction information for an AMP suggestion WHEN emitting an impression fact THEN 1 impression fact is collected`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuggestionImpressedFact(
                FxSuggestInteractionInfo.Amp(
                    blockId = 123,
                    advertiser = "mozilla",
                    reportingUrl = "https://example.com/reporting",
                    iabCategory = "22 - Shopping",
                    contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                ),
                positionInAwesomeBar = 0,
                isClicked = true,
            )

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_FXSUGGEST, component)
                assertEquals(Action.DISPLAY, action)
                assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

                assertEquals(
                    setOf(
                        FxSuggestFacts.MetadataKeys.INTERACTION_INFO,
                        FxSuggestFacts.MetadataKeys.POSITION,
                        FxSuggestFacts.MetadataKeys.IS_CLICKED,
                    ),
                    metadata?.keys,
                )

                val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
                assertEquals(impressionInfo.blockId, 123)
                assertEquals(impressionInfo.advertiser, "mozilla")
                assertEquals(impressionInfo.reportingUrl, "https://example.com/reporting")
                assertEquals(impressionInfo.iabCategory, "22 - Shopping")
                assertEquals(impressionInfo.contextId, "c303282d-f2e6-46ca-a04a-35d3d873712d")

                val positionInAwesomebar = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
                assertEquals(0, positionInAwesomebar)

                val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
                assertTrue(isClicked)
            }
        }
    }

    @Test
    fun `GIVEN interaction information for a Wikipedia suggestion WHEN emitting a click fact THEN 1 click fact is collected`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuggestionClickedFact(
                FxSuggestInteractionInfo.Wikipedia(
                    contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                ),
                positionInAwesomeBar = 0,
            )

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_FXSUGGEST, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(FxSuggestFacts.Items.WIKIPEDIA_SUGGESTION_CLICKED, item)

                assertEquals(
                    setOf(
                        FxSuggestFacts.MetadataKeys.INTERACTION_INFO,
                        FxSuggestFacts.MetadataKeys.POSITION,
                    ),
                    metadata?.keys,
                )

                val clickInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Wikipedia)
                assertEquals(clickInfo.contextId, "c303282d-f2e6-46ca-a04a-35d3d873712d")

                val positionInAwesomebar = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
                assertEquals(0, positionInAwesomebar)
            }
        }
    }

    @Test
    fun `GIVEN interaction information for a Wikipedia suggestion WHEN emitting an impression fact THEN 1 impression fact is collected`() {
        CollectionProcessor.withFactCollection { facts ->

            emitSuggestionImpressedFact(
                FxSuggestInteractionInfo.Wikipedia(
                    contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                ),
                positionInAwesomeBar = 0,
                isClicked = true,
            )

            assertEquals(1, facts.size)
            facts[0].apply {
                assertEquals(Component.FEATURE_FXSUGGEST, component)
                assertEquals(Action.DISPLAY, action)
                assertEquals(FxSuggestFacts.Items.WIKIPEDIA_SUGGESTION_IMPRESSED, item)

                assertEquals(
                    setOf(
                        FxSuggestFacts.MetadataKeys.INTERACTION_INFO,
                        FxSuggestFacts.MetadataKeys.POSITION,
                        FxSuggestFacts.MetadataKeys.IS_CLICKED,
                    ),
                    metadata?.keys,
                )

                val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Wikipedia)
                assertEquals(impressionInfo.contextId, "c303282d-f2e6-46ca-a04a-35d3d873712d")

                val positionInAwesomebar = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
                assertEquals(0, positionInAwesomebar)

                val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
                assertTrue(isClicked)
            }
        }
    }
}
