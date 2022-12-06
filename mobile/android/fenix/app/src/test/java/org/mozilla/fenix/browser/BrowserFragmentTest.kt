/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.browser.state.action.RestoreCompleteAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings

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
    private lateinit var navController: NavController
    private lateinit var onboarding: FenixOnboarding

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        fenixApplication = mockk(relaxed = true)
        every { context.application } returns fenixApplication

        homeActivity = mockk(relaxed = true)
        view = mockk(relaxed = true)
        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)
        navController = mockk(relaxed = true)
        onboarding = mockk(relaxed = true)

        browserFragment = spyk(BrowserFragment())
        every { browserFragment.view } returns view
        every { browserFragment.isAdded } returns true
        every { browserFragment.browserToolbarView } returns mockk(relaxed = true)
        every { browserFragment.activity } returns homeActivity
        every { browserFragment.lifecycle } returns lifecycleOwner.lifecycle
        every { browserFragment.onboarding } returns onboarding

        every { browserFragment.requireContext() } returns context
        every { browserFragment.initializeUI(any(), any()) } returns mockk()
        every { browserFragment.fullScreenChanged(any()) } returns Unit
        every { browserFragment.resumeDownloadDialogState(any(), any(), any(), any()) } returns Unit

        testTab = createTab(url = "https://mozilla.org")
        store = BrowserStore()
        every { context.components.core.store } returns store
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
        verify(exactly = 0) { browserFragment.initializeUI(view, testTab) }

        addAndSelectTab(testTab)
        verify(exactly = 1) { browserFragment.initializeUI(view, testTab) }
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
            browserFragment.resumeDownloadDialogState(newSelectedTab.id, store, context, any())
        }
    }

    @Test
    fun `GIVEN tabs are restored WHEN there are no tabs THEN navigate to home`() {
        browserFragment.observeRestoreComplete(store, navController)
        store.dispatch(RestoreCompleteAction).joinBlocking()

        verify(exactly = 1) { navController.popBackStack(R.id.homeFragment, false) }
    }

    @Test
    fun `GIVEN tabs are restored WHEN there are tabs THEN do not navigate`() {
        addAndSelectTab(testTab)
        browserFragment.observeRestoreComplete(store, navController)
        store.dispatch(RestoreCompleteAction).joinBlocking()

        verify(exactly = 0) { navController.popBackStack(R.id.homeFragment, false) }
    }

    @Test
    fun `GIVEN tabs are restored WHEN there is no selected tab THEN navigate to home`() {
        val store = BrowserStore(initialState = BrowserState(tabs = listOf(testTab)))
        browserFragment.observeRestoreComplete(store, navController)
        store.dispatch(RestoreCompleteAction).joinBlocking()

        verify(exactly = 1) { navController.popBackStack(R.id.homeFragment, false) }
    }

    @Test
    fun `GIVEN the onboarding is finished WHEN visiting any link THEN the onboarding is not dismissed `() {
        every { onboarding.userHasBeenOnboarded() } returns true

        browserFragment.observeTabSource(store)

        val newSelectedTab = createTab("any-tab.org")
        addAndSelectTab(newSelectedTab)

        verify(exactly = 0) { onboarding.finish() }
    }

    @Test
    fun `GIVEN the onboarding is not finished WHEN visiting a link THEN the onboarding is dismissed `() {
        every { onboarding.userHasBeenOnboarded() } returns false

        browserFragment.observeTabSource(store)

        val newSelectedTab = createTab("any-tab.org")
        addAndSelectTab(newSelectedTab)

        verify(exactly = 1) { onboarding.finish() }
    }

    @Test
    fun `GIVEN the onboarding is not finished WHEN visiting an onboarding link THEN the onboarding is not dismissed `() {
        every { onboarding.userHasBeenOnboarded() } returns false

        browserFragment.observeTabSource(store)

        val newSelectedTab = createTab(BaseBrowserFragment.onboardingLinksList[0])
        addAndSelectTab(newSelectedTab)

        verify(exactly = 0) { onboarding.finish() }
    }

    @Test
    fun `GIVEN the onboarding is not finished WHEN opening a page from another app THEN the onboarding is not dismissed `() {
        every { onboarding.userHasBeenOnboarded() } returns false

        browserFragment.observeTabSource(store)

        val newSelectedTab1 = createTab("any-tab-1.org", source = SessionState.Source.External.ActionSearch(mockk()))
        val newSelectedTab2 = createTab("any-tab-2.org", source = SessionState.Source.External.ActionView(mockk()))
        val newSelectedTab3 = createTab("any-tab-3.org", source = SessionState.Source.External.ActionSend(mockk()))
        val newSelectedTab4 = createTab("any-tab-4.org", source = SessionState.Source.External.CustomTab(mockk()))

        addAndSelectTab(newSelectedTab1)
        verify(exactly = 0) { onboarding.finish() }

        addAndSelectTab(newSelectedTab2)
        verify(exactly = 0) { onboarding.finish() }

        addAndSelectTab(newSelectedTab3)
        verify(exactly = 0) { onboarding.finish() }

        addAndSelectTab(newSelectedTab4)
        verify(exactly = 0) { onboarding.finish() }
    }

    @Test
    fun `GIVEN the onboarding is not finished WHEN visiting an link after redirect THEN the onboarding is not dismissed `() {
        every { onboarding.userHasBeenOnboarded() } returns false

        val newSelectedTab: TabSessionState = mockk(relaxed = true)
        every { newSelectedTab.content.loadRequest?.triggeredByRedirect } returns true

        browserFragment.observeTabSource(store)
        addAndSelectTab(newSelectedTab)

        verify(exactly = 0) { onboarding.finish() }
    }

    @Test
    fun `WHEN isPullToRefreshEnabledInBrowser is disabled THEN pull down refresh is disabled`() {
        every { context.settings().isPullToRefreshEnabledInBrowser } returns true
        assert(browserFragment.shouldPullToRefreshBeEnabled(false))

        every { context.settings().isPullToRefreshEnabledInBrowser } returns false
        assert(!browserFragment.shouldPullToRefreshBeEnabled(false))
    }

    @Test
    fun `WHEN in fullscreen THEN pull down refresh is disabled`() {
        every { context.settings().isPullToRefreshEnabledInBrowser } returns true
        assert(browserFragment.shouldPullToRefreshBeEnabled(false))
        assert(!browserFragment.shouldPullToRefreshBeEnabled(true))
    }

    @Test
    fun `WHEN fragment is not attached THEN toolbar invalidation does nothing`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)
        val browserToolbar: BrowserToolbar = mockk(relaxed = true)
        val toolbarIntegration: ToolbarIntegration = mockk(relaxed = true)
        every { browserToolbarView.view } returns browserToolbar
        every { browserToolbarView.toolbarIntegration } returns toolbarIntegration
        every { browserFragment.context } returns null
        browserFragment._browserToolbarView = browserToolbarView
        browserFragment.safeInvalidateBrowserToolbarView()

        verify(exactly = 0) { browserToolbar.invalidateActions() }
        verify(exactly = 0) { toolbarIntegration.invalidateMenu() }
    }

    @Test
    @Suppress("TooGenericExceptionCaught")
    fun `WHEN fragment is attached and toolbar view is null THEN toolbar invalidation is safe`() {
        every { browserFragment.context } returns mockk(relaxed = true)
        try {
            browserFragment.safeInvalidateBrowserToolbarView()
        } catch (e: Exception) {
            fail("Exception thrown when invalidating toolbar")
        }
    }

    @Test
    fun `WHEN fragment and view are attached THEN toolbar invalidation is triggered`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)
        val browserToolbar: BrowserToolbar = mockk(relaxed = true)
        val toolbarIntegration: ToolbarIntegration = mockk(relaxed = true)
        every { browserToolbarView.view } returns browserToolbar
        every { browserToolbarView.toolbarIntegration } returns toolbarIntegration
        every { browserFragment.context } returns mockk(relaxed = true)
        browserFragment._browserToolbarView = browserToolbarView
        browserFragment.safeInvalidateBrowserToolbarView()

        verify(exactly = 1) { browserToolbar.invalidateActions() }
        verify(exactly = 1) { toolbarIntegration.invalidateMenu() }
    }

    @Test
    fun `WHEN toolbar is initialized THEN onConfigurationChanged sets toolbar actions for size in fragment`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)

        browserFragment._browserToolbarView = null
        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 0) { browserFragment.onUpdateToolbarForConfigurationChange(any()) }
        verify(exactly = 0) { browserFragment.updateToolbarActions(any()) }

        browserFragment._browserToolbarView = browserToolbarView
        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 1) { browserFragment.onUpdateToolbarForConfigurationChange(any()) }
        verify(exactly = 1) { browserFragment.updateToolbarActions(any()) }
    }

    @Test
    fun `WHEN fragment configuration changed THEN menu is dismissed`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)
        every { browserFragment.context } returns null
        browserFragment._browserToolbarView = browserToolbarView

        browserFragment.onConfigurationChanged(mockk(relaxed = true))

        verify(exactly = 1) { browserToolbarView.dismissMenu() }
    }

    @Test
    fun `WHEN fragment configuration screen size changes between tablet and mobile size THEN tablet action items added and removed`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)
        val browserToolbar: BrowserToolbar = mockk(relaxed = true)
        browserFragment._browserToolbarView = browserToolbarView
        every { browserFragment.browserToolbarView.view } returns browserToolbar

        mockkObject(ThemeManager.Companion)
        every { ThemeManager.resolveAttribute(any(), context) } returns mockk(relaxed = true)

        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(context, any()) } returns mockk()

        every { browserFragment.resources.getBoolean(R.bool.tablet) } returns true
        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 3) { browserToolbar.addNavigationAction(any()) }

        every { browserFragment.resources.getBoolean(R.bool.tablet) } returns false
        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 3) { browserToolbar.removeNavigationAction(any()) }

        unmockkObject(ThemeManager.Companion)
        unmockkStatic(AppCompatResources::class)
    }

    @Test
    fun `WHEN fragment configuration change enables tablet size twice THEN tablet action items are only added once`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)
        val browserToolbar: BrowserToolbar = mockk(relaxed = true)
        browserFragment._browserToolbarView = browserToolbarView
        every { browserFragment.browserToolbarView.view } returns browserToolbar

        mockkObject(ThemeManager.Companion)
        every { ThemeManager.resolveAttribute(any(), context) } returns mockk(relaxed = true)

        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(context, any()) } returns mockk()

        every { browserFragment.resources.getBoolean(R.bool.tablet) } returns true
        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 3) { browserToolbar.addNavigationAction(any()) }

        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 3) { browserToolbar.addNavigationAction(any()) }

        unmockkObject(ThemeManager.Companion)
        unmockkStatic(AppCompatResources::class)
    }

    @Test
    fun `WHEN fragment configuration change sets mobile size twice THEN tablet action items are not added or removed`() {
        val browserToolbarView: BrowserToolbarView = mockk(relaxed = true)
        val browserToolbar: BrowserToolbar = mockk(relaxed = true)
        browserFragment._browserToolbarView = browserToolbarView
        every { browserFragment.browserToolbarView.view } returns browserToolbar

        mockkObject(ThemeManager.Companion)
        every { ThemeManager.resolveAttribute(any(), context) } returns mockk(relaxed = true)

        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(context, any()) } returns mockk()

        every { browserFragment.resources.getBoolean(R.bool.tablet) } returns false
        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 0) { browserToolbar.addNavigationAction(any()) }
        verify(exactly = 0) { browserToolbar.removeNavigationAction(any()) }

        browserFragment.onConfigurationChanged(mockk(relaxed = true))
        verify(exactly = 0) { browserToolbar.addNavigationAction(any()) }
        verify(exactly = 0) { browserToolbar.removeNavigationAction(any()) }

        unmockkObject(ThemeManager.Companion)
        unmockkStatic(AppCompatResources::class)
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

    @Test
    fun `WHEN updating the last browse activity THEN update the associated preference`() {
        val settings: Settings = mockk(relaxed = true)

        every { browserFragment.context } returns context
        every { context.settings() } returns settings

        browserFragment.updateLastBrowseActivity()

        verify(exactly = 1) { settings.lastBrowseActivity = any() }
    }
}
