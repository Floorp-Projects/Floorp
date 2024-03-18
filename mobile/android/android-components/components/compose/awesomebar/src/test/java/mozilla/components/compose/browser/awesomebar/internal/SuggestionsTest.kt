/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar.internal

import mozilla.components.concept.awesomebar.AwesomeBar.Suggestion
import mozilla.components.concept.awesomebar.AwesomeBar.SuggestionProvider
import mozilla.components.concept.awesomebar.AwesomeBar.SuggestionProviderGroup
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SuggestionsTest {
    @Test
    fun `GIVEN 1 suggestion in 1 group WHEN neither are visible THEN return an empty visibility state`() {
        val provider: SuggestionProvider = mock()

        val visibleItems = VisibleItems(
            suggestions = mapOf(SuggestionProviderGroup(listOf(provider)) to listOf(Suggestion(provider))),
            visibleItemKeys = emptyList(),
        )

        val visibilityState = visibleItems.toVisibilityState()
        assertTrue(visibilityState.visibleProviderGroups.isEmpty())
    }

    @Test
    fun `GIVEN 1 suggestion in 1 group WHEN both are visible THEN return a visibility state containing the suggestion`() {
        val provider: SuggestionProvider = mock()
        whenever(provider.id).thenReturn("provider")
        val providerGroup = SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(Suggestion(provider))

        val visibleItems = VisibleItems(
            suggestions = mapOf(providerGroup to providerGroupSuggestions),
            visibleItemKeys = listOf(
                ItemKey.SuggestionGroup(providerGroup.id),
                ItemKey.Suggestion(providerGroup.id, provider.id, providerGroupSuggestions[0].id),
            ),
        )

        val visibilityState = visibleItems.toVisibilityState()
        assertEquals(
            visibilityState.visibleProviderGroups,
            mapOf(
                providerGroup to providerGroupSuggestions,
            ),
        )
    }

    @Test
    fun `GIVEN 1 suggestion in 1 group WHEN only the suggestion is visible THEN return a visibility state containing the suggestion`() {
        val provider: SuggestionProvider = mock()
        whenever(provider.id).thenReturn("provider")
        val providerGroup = SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(Suggestion(provider))

        val visibleItems = VisibleItems(
            suggestions = mapOf(providerGroup to providerGroupSuggestions),
            visibleItemKeys = listOf(
                ItemKey.Suggestion(providerGroup.id, provider.id, providerGroupSuggestions[0].id),
            ),
        )

        val visibilityState = visibleItems.toVisibilityState()
        assertEquals(
            visibilityState.visibleProviderGroups,
            mapOf(
                providerGroup to providerGroupSuggestions,
            ),
        )
    }

    @Test
    fun `GIVEN 2 suggestions in 1 group WHEN all are visible THEN return a visibility state containing the suggestions`() {
        val provider: SuggestionProvider = mock()
        whenever(provider.id).thenReturn("provider")
        val providerGroup = SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(Suggestion(provider), Suggestion(provider))

        val visibleItems = VisibleItems(
            suggestions = mapOf(providerGroup to providerGroupSuggestions),
            visibleItemKeys = listOf(
                ItemKey.SuggestionGroup(providerGroup.id),
                ItemKey.Suggestion(providerGroup.id, provider.id, providerGroupSuggestions[0].id),
                ItemKey.Suggestion(providerGroup.id, provider.id, providerGroupSuggestions[1].id),
            ),
        )

        val visibilityState = visibleItems.toVisibilityState()
        assertEquals(
            visibilityState.visibleProviderGroups,
            mapOf(
                providerGroup to providerGroupSuggestions,
            ),
        )
    }

    @Test
    fun `GIVEN 2 suggestions in 2 groups WHEN 1 suggestion is visible in 1 group THEN return a visibility state containing the suggestion`() {
        val firstProvider: SuggestionProvider = mock()
        whenever(firstProvider.id).thenReturn("firstProvider")
        val secondProvider: SuggestionProvider = mock()
        whenever(secondProvider.id).thenReturn("secondProvider")
        val thirdProvider: SuggestionProvider = mock()
        whenever(thirdProvider.id).thenReturn("thirdProvider")
        val firstProviderGroup = SuggestionProviderGroup(listOf(firstProvider, secondProvider))
        val firstProviderGroupSuggestions = listOf(Suggestion(firstProvider), Suggestion(secondProvider))
        val secondProviderGroup = SuggestionProviderGroup(listOf(secondProvider, thirdProvider))
        val secondProviderGroupSuggestions = listOf(Suggestion(secondProvider), Suggestion(thirdProvider))

        val visibleItems = VisibleItems(
            suggestions = mapOf(
                firstProviderGroup to firstProviderGroupSuggestions,
                secondProviderGroup to secondProviderGroupSuggestions,
            ),
            visibleItemKeys = listOf(
                ItemKey.SuggestionGroup(firstProviderGroup.id),
                ItemKey.Suggestion(firstProviderGroup.id, secondProvider.id, firstProviderGroupSuggestions[1].id),
            ),
        )

        val visibilityState = visibleItems.toVisibilityState()
        assertEquals(
            visibilityState.visibleProviderGroups,
            mapOf(
                firstProviderGroup to listOf(firstProviderGroupSuggestions[1]),
            ),
        )
    }

    @Test
    fun `GIVEN 2 suggestions in 2 groups WHEN 1 suggestion is visible in each group THEN return a visibility state containing the suggestions`() {
        val firstProvider: SuggestionProvider = mock()
        whenever(firstProvider.id).thenReturn("firstProvider")

        val secondProvider: SuggestionProvider = mock()
        whenever(secondProvider.id).thenReturn("secondProvider")

        val thirdProvider: SuggestionProvider = mock()
        whenever(thirdProvider.id).thenReturn("thirdProvider")

        val firstProviderGroup = SuggestionProviderGroup(listOf(firstProvider, secondProvider))
        val firstProviderGroupSuggestions = listOf(Suggestion(firstProvider), Suggestion(secondProvider))

        val secondProviderGroup = SuggestionProviderGroup(listOf(secondProvider, thirdProvider))
        val secondProviderGroupSuggestions = listOf(Suggestion(secondProvider), Suggestion(thirdProvider))

        val visibleItems = VisibleItems(
            suggestions = mapOf(
                firstProviderGroup to firstProviderGroupSuggestions,
                secondProviderGroup to secondProviderGroupSuggestions,
            ),
            visibleItemKeys = listOf(
                ItemKey.SuggestionGroup(firstProviderGroup.id),
                ItemKey.Suggestion(firstProviderGroup.id, firstProvider.id, firstProviderGroupSuggestions[0].id),
                ItemKey.SuggestionGroup(secondProviderGroup.id),
                ItemKey.Suggestion(secondProviderGroup.id, thirdProvider.id, secondProviderGroupSuggestions[1].id),
            ),
        )
        val visibilityState = visibleItems.toVisibilityState()
        assertEquals(
            visibilityState.visibleProviderGroups,
            mapOf(
                firstProviderGroup to listOf(firstProviderGroupSuggestions[0]),
                secondProviderGroup to listOf(secondProviderGroupSuggestions[1]),
            ),
        )
    }
}
