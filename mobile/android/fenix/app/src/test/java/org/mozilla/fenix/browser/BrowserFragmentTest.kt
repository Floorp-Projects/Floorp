/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.LoadRequestState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class BrowserFragmentTest {

    private lateinit var store: BrowserStore
    private lateinit var testTab: TabSessionState
    private lateinit var browserFragment: BrowserFragment
    private lateinit var view: View
    private lateinit var homeActivity: HomeActivity
    private lateinit var fenixApplication: FenixApplication
    private lateinit var context: Context
    private lateinit var lifecycleOwner: MockedLifecycleOwner

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        fenixApplication = mockk(relaxed = true)
        every { context.application } returns fenixApplication

        homeActivity = mockk(relaxed = true)
        view = mockk(relaxed = true)
        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        browserFragment = spyk(BrowserFragment())
        every { browserFragment.view } returns view
        every { browserFragment.isAdded } returns true
        every { browserFragment.browserToolbarView } returns mockk(relaxed = true)
        every { browserFragment.activity } returns homeActivity
        every { browserFragment.lifecycle } returns lifecycleOwner.lifecycle
        every { browserFragment.requireContext() } returns context
        every { browserFragment.initializeUI(any()) } returns mockk()
        every { browserFragment.fullScreenChanged(any()) } returns Unit
        every { browserFragment.resumeDownloadDialogState(any(), any(), any(), any(), any()) } returns Unit

        store = BrowserStore()
        every { context.components.core.store } returns store
        testTab = createTab(url = "https://mozilla.org")
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `GIVEN fragment is added WHEN selected tab changes THEN theme is updated`() {
        browserFragment.observeTabSelection(store)
        verify(exactly = 0) { browserFragment.updateThemeForSession(testTab) }

        addAndSelectTab(testTab)
        verify(exactly = 1) { browserFragment.updateThemeForSession(testTab) }
    }

    @Test
    fun `GIVEN fragment is removing WHEN selected tab changes THEN theme is not updated`() {
        every { browserFragment.isRemoving } returns true
        browserFragment.observeTabSelection(store)

        addAndSelectTab(testTab)
        verify(exactly = 0) { browserFragment.updateThemeForSession(testTab) }
    }

    @Test
    fun `GIVEN browser UI is not initialized WHEN selected tab changes THEN browser UI is initialized`() {
        browserFragment.observeTabSelection(store)
        verify(exactly = 0) { browserFragment.initializeUI(view) }

        addAndSelectTab(testTab)
        verify(exactly = 1) { browserFragment.initializeUI(view) }
    }

    @Test
    fun `GIVEN browser UI is initialized WHEN selected tab changes THEN toolbar is expanded`() {
        browserFragment.browserInitialized = true
        browserFragment.observeTabSelection(store)

        val toolbar: BrowserToolbarView = mockk(relaxed = true)
        every { browserFragment.browserToolbarView } returns toolbar

        val newSelectedTab = createTab("https://firefox.com")
        addAndSelectTab(newSelectedTab)
        verify(exactly = 1) { toolbar.expand() }
    }

    @Test
    fun `GIVEN browser UI is initialized WHEN selected tab changes THEN full screen mode is exited`() {
        browserFragment.browserInitialized = true
        browserFragment.observeTabSelection(store)

        val newSelectedTab = createTab("https://firefox.com")
        addAndSelectTab(newSelectedTab)
        verify(exactly = 1) { browserFragment.fullScreenChanged(false) }
    }

    @Test
    fun `GIVEN browser UI is initialized WHEN selected tab changes THEN download dialog is resumed`() {
        browserFragment.browserInitialized = true
        browserFragment.observeTabSelection(store)

        val newSelectedTab = createTab("https://firefox.com")
        addAndSelectTab(newSelectedTab)
        verify(exactly = 1) {
            browserFragment.resumeDownloadDialogState(newSelectedTab.id, store, view, context, any())
        }
    }

    @Test
    fun `WHEN url changes THEN toolbar is expanded`() {
        addAndSelectTab(testTab)
        browserFragment.expandToolbarOnNavigation(store)

        val toolbar: BrowserToolbarView = mockk(relaxed = true)
        every { browserFragment.browserToolbarView } returns toolbar

        store.dispatch(ContentAction.UpdateUrlAction(testTab.id, "https://firefox.com")).joinBlocking()
        verify(exactly = 1) { toolbar.expand() }
    }

    @Test
    fun `WHEN load request is triggered THEN toolbar is expanded`() {
        addAndSelectTab(testTab)
        browserFragment.expandToolbarOnNavigation(store)

        val toolbar: BrowserToolbarView = mockk(relaxed = true)
        every { browserFragment.browserToolbarView } returns toolbar

        store.dispatch(ContentAction.UpdateLoadRequestAction(
            testTab.id,
            LoadRequestState("https://firefox.com", false, true))
        ).joinBlocking()
        verify(exactly = 1) { toolbar.expand() }
    }

    private fun addAndSelectTab(tab: TabSessionState) {
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }
}
