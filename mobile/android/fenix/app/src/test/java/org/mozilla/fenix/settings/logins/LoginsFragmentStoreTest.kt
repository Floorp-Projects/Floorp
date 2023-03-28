/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.every
import io.mockk.mockk
import mozilla.components.concept.storage.Login
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.utils.Settings

class LoginsFragmentStoreTest {

    private val baseLogin = SavedLogin(
        guid = "",
        origin = "",
        username = "",
        password = "",
        timeLastUsed = 0L,
    )
    private val exampleLogin = baseLogin.copy(origin = "example.com", timeLastUsed = 10)
    private val firefoxLogin = baseLogin.copy(origin = "firefox.com", timeLastUsed = 20)
    private val loginList = listOf(exampleLogin, firefoxLogin)
    private val baseState = LoginsListState(
        loginList = emptyList(),
        filteredItems = emptyList(),
        searchedForText = null,
        sortingStrategy = SortingStrategy.LastUsed,
        highlightedItem = SavedLoginsSortingStrategyMenu.Item.LastUsedSort,
        duplicateLogin = null,
    )

    @Test
    fun `create initial state`() {
        val settings = mockk<Settings>()
        every { settings.savedLoginsSortingStrategy } returns SortingStrategy.LastUsed
        every { settings.savedLoginsMenuHighlightedItem } returns SavedLoginsSortingStrategyMenu.Item.LastUsedSort

        assertEquals(
            LoginsListState(
                isLoading = true,
                loginList = emptyList(),
                filteredItems = emptyList(),
                searchedForText = null,
                sortingStrategy = SortingStrategy.LastUsed,
                highlightedItem = SavedLoginsSortingStrategyMenu.Item.LastUsedSort,
                duplicateLogin = null,
            ),
            createInitialLoginsListState(settings),
        )
    }

    @Test
    fun `convert login to saved login`() {
        val login = Login(
            guid = "abcd",
            origin = "example.com",
            username = "login",
            password = "password",
            timeLastUsed = 35L,
        ).mapToSavedLogin()

        assertEquals("abcd", login.guid)
        assertEquals("example.com", login.origin)
        assertEquals("login", login.username)
        assertEquals("password", login.password)
        assertEquals(35L, login.timeLastUsed)
    }

    @Test
    fun `UpdateLoginsList action`() {
        val store = LoginsFragmentStore(baseState.copy(isLoading = true))

        store.dispatch(LoginsAction.UpdateLoginsList(loginList)).joinBlocking()

        assertFalse(store.state.isLoading)
        assertEquals(loginList, store.state.loginList)
        assertEquals(listOf(firefoxLogin, exampleLogin), store.state.filteredItems)
    }

    @Test
    fun `GIVEN logins already exist WHEN asked to add a new one THEN update the state to contain them all`() {
        val store = LoginsFragmentStore(
            baseState.copy(isLoading = true),
        )

        store.dispatch(LoginsAction.AddLogin(exampleLogin)).joinBlocking()
        assertFalse(store.state.isLoading)
        assertEquals(listOf(exampleLogin), store.state.loginList)

        // Select a login to force "isLoading = true"
        store.dispatch(LoginsAction.AddLogin(firefoxLogin)).joinBlocking()
        assertFalse(store.state.isLoading)
        assertEquals(loginList, store.state.loginList)
        assertEquals(listOf(firefoxLogin, exampleLogin), store.state.filteredItems)
    }

    @Test
    fun `FilterLogins action`() {
        val store = LoginsFragmentStore(
            baseState.copy(
                isLoading = true,
                searchedForText = "firefox",
                loginList = loginList,
            ),
        )

        store.dispatch(LoginsAction.FilterLogins(null)).joinBlocking()

        assertFalse(store.state.isLoading)
        assertNull(store.state.searchedForText)
        assertEquals(listOf(firefoxLogin, exampleLogin), store.state.filteredItems)
    }

    @Test
    fun `UpdateCurrentLogin action`() {
        val store = LoginsFragmentStore(baseState.copy(isLoading = true))

        store.dispatch(LoginsAction.UpdateCurrentLogin(baseLogin)).joinBlocking()

        assertEquals(baseLogin, store.state.currentItem)
    }

    @Test
    fun `SortLogins action`() {
        val lastUsed = SortingStrategy.LastUsed
        val store = LoginsFragmentStore(
            baseState.copy(
                isLoading = true,
                searchedForText = null,
                sortingStrategy = SortingStrategy.Alphabetically,
                highlightedItem = SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort,
                loginList = loginList,
            ),
        )

        store.dispatch(LoginsAction.SortLogins(lastUsed)).joinBlocking()

        assertFalse(store.state.isLoading)
        assertEquals(lastUsed, store.state.sortingStrategy)
        assertEquals(SavedLoginsSortingStrategyMenu.Item.LastUsedSort, store.state.highlightedItem)
        assertNull(store.state.searchedForText)
        assertEquals(listOf(firefoxLogin, exampleLogin), store.state.filteredItems)
    }

    @Test
    fun `SortLogins action with search text`() {
        val lastUsed = SortingStrategy.LastUsed
        val store = LoginsFragmentStore(
            baseState.copy(
                isLoading = true,
                searchedForText = "example",
                sortingStrategy = SortingStrategy.Alphabetically,
                highlightedItem = SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort,
                loginList = loginList,
            ),
        )

        store.dispatch(LoginsAction.SortLogins(lastUsed)).joinBlocking()

        assertFalse(store.state.isLoading)
        assertEquals(lastUsed, store.state.sortingStrategy)
        assertEquals(SavedLoginsSortingStrategyMenu.Item.LastUsedSort, store.state.highlightedItem)
        assertEquals("example", store.state.searchedForText)
        assertEquals(listOf(exampleLogin), store.state.filteredItems)
    }

    @Test
    fun `LoginSelected action`() {
        val store = LoginsFragmentStore(
            baseState.copy(
                isLoading = false,
                loginList = listOf(mockk()),
                filteredItems = listOf(mockk()),
            ),
        )

        store.dispatch(LoginsAction.LoginSelected(mockk())).joinBlocking()

        assertTrue(store.state.isLoading)
        assertTrue(store.state.loginList.isNotEmpty())
        assertTrue(store.state.filteredItems.isNotEmpty())
    }
}
