/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.toolbar

import android.content.Context
import android.net.Uri
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.toolbar.DefaultToolbarMenu

@ExperimentalCoroutinesApi
class DefaultToolbarMenuTest {

    private lateinit var store: BrowserStore
    private lateinit var lifecycleOwner: MockedLifecycleOwner
    private lateinit var toolbarMenu: DefaultToolbarMenu
    private lateinit var context: Context
    private lateinit var bookmarksStorage: BookmarksStorage

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setUp() {
        mockkStatic(Uri::class)
        every { Uri.parse(any()) } returns mockk(relaxed = true)

        context = mockk(relaxed = true)
        every { context.theme } returns mockk(relaxed = true)
        bookmarksStorage = mockk(relaxed = true)
        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(url = "https://firefox.com", id = "1"),
                    createTab(url = "https://getpocket.com", id = "2")
                ), selectedTabId = "1"
            )
        )
        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        toolbarMenu = spyk(DefaultToolbarMenu(
            context = context,
            store = store,
            hasAccountProblem = false,
            shouldReverseItems = false,
            onItemTapped = { },
            lifecycleOwner = lifecycleOwner,
            bookmarksStorage = bookmarksStorage,
            isPinningSupported = false
        ))

        every { toolbarMenu.updateCurrentUrlIsBookmarked(any()) } returns Unit
    }

    @After
    fun tearDown() {
        unmockkStatic(Uri::class)
    }

    @Test
    fun `WHEN url changes THEN bookmarked state is updated`() {
        toolbarMenu.registerForIsBookmarkedUpdates()

        val newUrl = "https://mozilla.org"

        store.dispatch(ContentAction.UpdateUrlAction("1", newUrl)).joinBlocking()
        verify(exactly = 1) { toolbarMenu.updateCurrentUrlIsBookmarked(newUrl) }
    }

    @Test
    fun `WHEN selected tab changes THEN bookmarked state is updated`() {
        toolbarMenu.registerForIsBookmarkedUpdates()

        val newSelectedTab = store.state.findTab("2")
        assertNotNull(newSelectedTab)

        store.dispatch(TabListAction.SelectTabAction(newSelectedTab!!.id)).joinBlocking()
        verify(exactly = 1) { toolbarMenu.updateCurrentUrlIsBookmarked(newSelectedTab.content.url) }
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }
}
