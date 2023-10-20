/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.state.AwesomeBarState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.fxsuggest.facts.FxSuggestFacts
import mozilla.components.feature.fxsuggest.facts.FxSuggestFactsMiddleware
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class FxSuggestFactsMiddlewareTest {
    private lateinit var processor: CollectionProcessor

    @Before
    fun setUp() {
        processor = CollectionProcessor()
        Facts.registerProcessor(processor)
    }

    @After
    fun tearDown() {
        Facts.clearProcessors()
    }

    @Test
    fun `GIVEN no suggestions are visible WHEN the engagement is completed THEN no facts are collected`() {
        val store = BrowserStore(
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertTrue(processor.facts.isEmpty())
    }

    @Test
    fun `GIVEN 2 non-AMP suggestions are visible WHEN the engagement is completed THEN no facts are collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(provider),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = providerGroupSuggestions[1],
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertTrue(processor.facts.isEmpty())
    }

    @Test
    fun `GIVEN 1 AMP suggestion is visible WHEN the engagement is abandoned THEN no facts are collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = providerGroupSuggestions[1],
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true)).joinBlocking()

        assertTrue(processor.facts.isEmpty())
    }

    @Test
    fun `GIVEN 1 AMP suggestion is visible WHEN the engagement is completed THEN 1 impression fact is collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertEquals(1, processor.facts.size)
        processor.facts[0].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, impressionInfo.blockId)
            assertEquals("mozilla", impressionInfo.advertiser)
            assertEquals("https://example.com/impression", impressionInfo.reportingUrl)
            assertEquals("22 - Shopping", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
    }

    @Test
    fun `GIVEN 1 AMP suggestion is visible and a non-AMP suggestion is clicked WHEN the engagement is completed THEN 1 impression fact is collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = providerGroupSuggestions[0],
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertEquals(1, processor.facts.size)
        processor.facts[0].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, impressionInfo.blockId)
            assertEquals("mozilla", impressionInfo.advertiser)
            assertEquals("https://example.com/impression", impressionInfo.reportingUrl)
            assertEquals("22 - Shopping", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
    }

    @Test
    fun `GIVEN 1 AMP suggestion is visible and clicked WHEN the engagement is completed THEN 1 impression fact and 1 click fact are collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = providerGroupSuggestions[1],
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertEquals(2, processor.facts.size)
        processor.facts[0].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, impressionInfo.blockId)
            assertEquals("mozilla", impressionInfo.advertiser)
            assertEquals("https://example.com/impression", impressionInfo.reportingUrl)
            assertEquals("22 - Shopping", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertTrue(isClicked)
        }
        processor.facts[1].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.INTERACTION, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_CLICKED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION), metadata?.keys)

            val clickInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, clickInfo.blockId)
            assertEquals("mozilla", clickInfo.advertiser)
            assertEquals("https://example.com/click", clickInfo.reportingUrl)
            assertEquals("22 - Shopping", clickInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", clickInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)
        }
    }

    @Test
    fun `GIVEN 2 AMP suggestions are visible WHEN the engagement is completed THEN 2 impression facts are collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression-1",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click-1",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 456,
                        advertiser = "good place eats",
                        reportingUrl = "https://example.com/impression-2",
                        iabCategory = "8 - Food & Drink",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 456,
                        advertiser = "good place eats",
                        reportingUrl = "https://example.com/click-2",
                        iabCategory = "8 - Food & Drink",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertEquals(2, processor.facts.size)
        processor.facts[0].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, impressionInfo.blockId)
            assertEquals("mozilla", impressionInfo.advertiser)
            assertEquals("https://example.com/impression-1", impressionInfo.reportingUrl)
            assertEquals("22 - Shopping", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
        processor.facts[1].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(456, impressionInfo.blockId)
            assertEquals("good place eats", impressionInfo.advertiser)
            assertEquals("https://example.com/impression-2", impressionInfo.reportingUrl)
            assertEquals("8 - Food & Drink", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(4, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
    }

    @Test
    fun `GIVEN 2 AMP suggestions are visible and a non-AMP suggestion is clicked WHEN the engagement is completed THEN 2 impression facts are collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression-1",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click-1",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 456,
                        advertiser = "good place eats",
                        reportingUrl = "https://example.com/impression-2",
                        iabCategory = "8 - Food & Drink",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 456,
                        advertiser = "good place eats",
                        reportingUrl = "https://example.com/click-2",
                        iabCategory = "8 - Food & Drink",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = providerGroupSuggestions[2],
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertEquals(2, processor.facts.size)
        processor.facts[0].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, impressionInfo.blockId)
            assertEquals("mozilla", impressionInfo.advertiser)
            assertEquals("https://example.com/impression-1", impressionInfo.reportingUrl)
            assertEquals("22 - Shopping", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
        processor.facts[1].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(456, impressionInfo.blockId)
            assertEquals("good place eats", impressionInfo.advertiser)
            assertEquals("https://example.com/impression-2", impressionInfo.reportingUrl)
            assertEquals("8 - Food & Drink", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(4, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
    }

    @Test
    fun `GIVEN 2 AMP suggestions are visible and an AMP suggestion is clicked WHEN the engagement is completed THEN 2 impression facts and 1 click fact are collected`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/impression-1",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 123,
                        advertiser = "mozilla",
                        reportingUrl = "https://example.com/click-1",
                        iabCategory = "22 - Shopping",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
            AwesomeBar.Suggestion(provider),
            AwesomeBar.Suggestion(
                provider = provider,
                metadata = mapOf(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 456,
                        advertiser = "good place eats",
                        reportingUrl = "https://example.com/impression-2",
                        iabCategory = "8 - Food & Drink",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                    FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO to FxSuggestInteractionInfo.Amp(
                        blockId = 456,
                        advertiser = "good place eats",
                        reportingUrl = "https://example.com/click-2",
                        iabCategory = "8 - Food & Drink",
                        contextId = "c303282d-f2e6-46ca-a04a-35d3d873712d",
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = providerGroupSuggestions[3],
                ),
            ),
            middleware = listOf(FxSuggestFactsMiddleware()),
        )

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertEquals(3, processor.facts.size)
        processor.facts[0].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(123, impressionInfo.blockId)
            assertEquals("mozilla", impressionInfo.advertiser)
            assertEquals("https://example.com/impression-1", impressionInfo.reportingUrl)
            assertEquals("22 - Shopping", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(2, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertFalse(isClicked)
        }
        processor.facts[1].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.DISPLAY, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION, FxSuggestFacts.MetadataKeys.IS_CLICKED), metadata?.keys)

            val impressionInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(456, impressionInfo.blockId)
            assertEquals("good place eats", impressionInfo.advertiser)
            assertEquals("https://example.com/impression-2", impressionInfo.reportingUrl)
            assertEquals("8 - Food & Drink", impressionInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", impressionInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(4, position)

            val isClicked = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.IS_CLICKED) as? Boolean)
            assertTrue(isClicked)
        }
        processor.facts[2].apply {
            assertEquals(Component.FEATURE_FXSUGGEST, component)
            assertEquals(Action.INTERACTION, action)
            assertEquals(FxSuggestFacts.Items.AMP_SUGGESTION_CLICKED, item)

            assertEquals(setOf(FxSuggestFacts.MetadataKeys.INTERACTION_INFO, FxSuggestFacts.MetadataKeys.POSITION), metadata?.keys)

            val clickInfo = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.INTERACTION_INFO) as? FxSuggestInteractionInfo.Amp)
            assertEquals(456, clickInfo.blockId)
            assertEquals("good place eats", clickInfo.advertiser)
            assertEquals("https://example.com/click-2", clickInfo.reportingUrl)
            assertEquals("8 - Food & Drink", clickInfo.iabCategory)
            assertEquals("c303282d-f2e6-46ca-a04a-35d3d873712d", clickInfo.contextId)

            val position = requireNotNull(metadata?.get(FxSuggestFacts.MetadataKeys.POSITION) as? Long)
            assertEquals(4, position)
        }
    }
}
