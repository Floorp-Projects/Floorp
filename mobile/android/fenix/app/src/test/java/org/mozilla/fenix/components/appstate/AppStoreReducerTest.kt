/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import io.mockk.mockk
import mozilla.components.browser.state.state.createTab
import mozilla.components.lib.crash.Crash.NativeCodeCrash
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppAction.AddNonFatalCrash
import org.mozilla.fenix.components.appstate.AppAction.RemoveAllNonFatalCrashes
import org.mozilla.fenix.components.appstate.AppAction.RemoveNonFatalCrash
import org.mozilla.fenix.components.appstate.AppAction.UpdateInactiveExpanded

class AppStoreReducerTest {
    @Test
    fun `GIVEN a new value for inactiveTabsExpanded WHEN UpdateInactiveExpanded is called THEN update the current value`() {
        val initialState = AppState(
            inactiveTabsExpanded = true,
        )

        var updatedState = AppStoreReducer.reduce(initialState, UpdateInactiveExpanded(false))
        assertFalse(updatedState.inactiveTabsExpanded)

        updatedState = AppStoreReducer.reduce(updatedState, UpdateInactiveExpanded(true))
        assertTrue(updatedState.inactiveTabsExpanded)
    }

    @Test
    fun `GIVEN a Crash WHEN AddNonFatalCrash is called THEN add that Crash to the current list`() {
        val initialState = AppState()
        val crash1: NativeCodeCrash = mockk()
        val crash2: NativeCodeCrash = mockk()

        var updatedState = AppStoreReducer.reduce(initialState, AddNonFatalCrash(crash1))
        assertTrue(listOf(crash1).containsAll(updatedState.nonFatalCrashes))

        updatedState = AppStoreReducer.reduce(updatedState, AddNonFatalCrash(crash2))
        assertTrue(listOf(crash1, crash2).containsAll(updatedState.nonFatalCrashes))
    }

    @Test
    fun `GIVEN a Crash WHEN RemoveNonFatalCrash is called THEN remove that Crash from the current list`() {
        val crash1: NativeCodeCrash = mockk()
        val crash2: NativeCodeCrash = mockk()
        val initialState = AppState(
            nonFatalCrashes = listOf(crash1, crash2),
        )

        var updatedState = AppStoreReducer.reduce(initialState, RemoveNonFatalCrash(crash1))
        assertTrue(listOf(crash2).containsAll(updatedState.nonFatalCrashes))

        updatedState = AppStoreReducer.reduce(updatedState, RemoveNonFatalCrash(mockk()))
        assertTrue(listOf(crash2).containsAll(updatedState.nonFatalCrashes))

        updatedState = AppStoreReducer.reduce(updatedState, RemoveNonFatalCrash(crash2))
        assertTrue(updatedState.nonFatalCrashes.isEmpty())
    }

    @Test
    fun `GIVEN crashes exist in State WHEN RemoveAllNonFatalCrashes is called THEN clear the current list of crashes`() {
        val initialState = AppState(
            nonFatalCrashes = listOf(mockk(), mockk()),
        )

        val updatedState = AppStoreReducer.reduce(initialState, RemoveAllNonFatalCrashes)

        assertTrue(updatedState.nonFatalCrashes.isEmpty())
    }

    @Test
    fun `GIVEN mode is private WHEN selected tab changes to normal mode THEN state is updated to normal mode`() {
        val initialState = AppState(
            selectedTabId = null,
            mode = BrowsingMode.Private,
        )

        val updatedState = AppStoreReducer.reduce(
            initialState,
            AppAction.SelectedTabChanged(createTab("", private = false)),
        )

        assertFalse(updatedState.mode.isPrivate)
    }

    @Test
    fun `GIVEN mode is normal WHEN selected tab changes to private mode THEN state is updated to private mode`() {
        val initialState = AppState(
            selectedTabId = null,
            mode = BrowsingMode.Normal,
        )

        val updatedState = AppStoreReducer.reduce(
            initialState,
            AppAction.SelectedTabChanged(createTab("", private = true)),
        )

        assertTrue(updatedState.mode.isPrivate)
    }

    @Test
    fun `WHEN selected tab changes to a tab in the same mode THEN mode is unchanged`() {
        val initialState = AppState(
            selectedTabId = null,
            mode = BrowsingMode.Normal,
        )

        val updatedState = AppStoreReducer.reduce(
            initialState,
            AppAction.SelectedTabChanged(createTab("", private = false)),
        )

        assertFalse(updatedState.mode.isPrivate)
    }
}
