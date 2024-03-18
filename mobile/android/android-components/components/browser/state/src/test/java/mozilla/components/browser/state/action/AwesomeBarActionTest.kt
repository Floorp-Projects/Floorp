/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.AwesomeBarState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AwesomeBarActionTest {
    @Test
    fun `VisibilityStateUpdated - Stores updated visibility state`() {
        val store = BrowserStore()

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isEmpty())
        assertNull(store.state.awesomeBarState.clickedSuggestion)

        val provider: AwesomeBar.SuggestionProvider = mock()
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(AwesomeBar.Suggestion(provider))

        store.dispatch(
            AwesomeBarAction.VisibilityStateUpdated(
                AwesomeBar.VisibilityState(
                    visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                ),
            ),
        ).joinBlocking()

        assertEquals(1, store.state.awesomeBarState.visibilityState.visibleProviderGroups.size)
        assertEquals(providerGroupSuggestions, store.state.awesomeBarState.visibilityState.visibleProviderGroups[providerGroup])
        assertNull(store.state.awesomeBarState.clickedSuggestion)
    }

    @Test
    fun `SuggestionClicked - Stores clicked suggestion`() {
        val store = BrowserStore()

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isEmpty())
        assertNull(store.state.awesomeBarState.clickedSuggestion)

        val provider: AwesomeBar.SuggestionProvider = mock()
        val suggestion = AwesomeBar.Suggestion(provider)

        store.dispatch(AwesomeBarAction.SuggestionClicked(suggestion)).joinBlocking()

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isEmpty())
        assertEquals(suggestion, store.state.awesomeBarState.clickedSuggestion)
    }

    @Test
    fun `EngagementFinished - Completed engagement resets state`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val suggestion = AwesomeBar.Suggestion(provider)
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(suggestion)
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = suggestion,
                ),
            ),
        )

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isNotEmpty())
        assertNotNull(store.state.awesomeBarState.clickedSuggestion)

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false)).joinBlocking()

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isEmpty())
        assertNull(store.state.awesomeBarState.clickedSuggestion)
    }

    @Test
    fun `EngagementFinished - Abandoned engagement resets state`() {
        val provider: AwesomeBar.SuggestionProvider = mock()
        val suggestion = AwesomeBar.Suggestion(provider)
        val providerGroup = AwesomeBar.SuggestionProviderGroup(listOf(provider))
        val providerGroupSuggestions = listOf(suggestion)
        val store = BrowserStore(
            initialState = BrowserState(
                awesomeBarState = AwesomeBarState(
                    visibilityState = AwesomeBar.VisibilityState(
                        visibleProviderGroups = mapOf(providerGroup to providerGroupSuggestions),
                    ),
                    clickedSuggestion = suggestion,
                ),
            ),
        )

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isNotEmpty())
        assertNotNull(store.state.awesomeBarState.clickedSuggestion)

        store.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true)).joinBlocking()

        assertTrue(store.state.awesomeBarState.visibilityState.visibleProviderGroups.isEmpty())
        assertNull(store.state.awesomeBarState.clickedSuggestion)
    }
}
