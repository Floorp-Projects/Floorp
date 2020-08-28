/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import androidx.navigation.NavController
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.utils.Settings

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class DefaultBrowserToolbarMenuControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @MockK private lateinit var swipeRefreshLayout: SwipeRefreshLayout
    @RelaxedMockK private lateinit var activity: HomeActivity
    @RelaxedMockK private lateinit var navController: NavController
    @RelaxedMockK private lateinit var findInPageLauncher: () -> Unit
    @RelaxedMockK private lateinit var bookmarkTapped: (Session) -> Unit
    @RelaxedMockK private lateinit var sessionManager: SessionManager
    @RelaxedMockK private lateinit var currentSession: Session
    @RelaxedMockK private lateinit var openInFenixIntent: Intent
    @RelaxedMockK private lateinit var metrics: MetricController
    @RelaxedMockK private lateinit var settings: Settings
    @RelaxedMockK private lateinit var searchUseCases: SearchUseCases
    @RelaxedMockK private lateinit var sessionUseCases: SessionUseCases
    @RelaxedMockK private lateinit var browserAnimator: BrowserAnimator
    @RelaxedMockK private lateinit var snackbar: FenixSnackbar
    @RelaxedMockK private lateinit var tabCollectionStorage: TabCollectionStorage
    @RelaxedMockK private lateinit var topSitesUseCase: TopSitesUseCases
    @RelaxedMockK private lateinit var readerModeController: ReaderModeController
    @MockK private lateinit var sessionFeatureWrapper: ViewBoundFeatureWrapper<SessionFeature>
    @RelaxedMockK private lateinit var sessionFeature: SessionFeature

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        mockkStatic(
            "org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt"
        )
        every { deleteAndQuit(any(), any(), any()) } just Runs

        mockkObject(FenixSnackbar.Companion)
        every { FenixSnackbar.make(any(), any(), any(), any()) } returns snackbar

        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.useCases.searchUseCases } returns searchUseCases
        every { activity.components.useCases.topSitesUseCase } returns topSitesUseCase
        every { sessionManager.selectedSession } returns currentSession
        every { sessionFeatureWrapper.get() } returns sessionFeature
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.browserFragment
        }
        every { currentSession.id } returns "1"

        val onComplete = slot<() -> Unit>()
        every { browserAnimator.captureEngineViewAndDrawStatically(capture(onComplete)) } answers { onComplete.captured.invoke() }
    }

    @After
    fun tearDown() {
        unmockkStatic("org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt")
        unmockkObject(FenixSnackbar.Companion)
    }

    @Test
    fun handleToolbarBackPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Back(false)

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BACK)) }
        verify { sessionUseCases.goBack(currentSession) }
    }

    @Test
    fun handleToolbarBackLongPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Back(true)

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment()

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BACK)) }
        verify { navController.navigate(directions) }
    }

    @Test
    fun handleToolbarForwardPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Forward(false)

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.FORWARD)) }
        verify { sessionUseCases.goForward(currentSession) }
    }

    @Test
    fun handleToolbarForwardLongPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Forward(true)

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment()

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.FORWARD)) }
        verify { navController.navigate(directions) }
    }

    @Test
    fun handleToolbarReloadPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Reload(false)

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.RELOAD)) }
        verify { sessionUseCases.reload(currentSession) }
    }

    @Test
    fun handleToolbarReloadLongPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Reload(true)

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.RELOAD)) }
        verify {
            sessionUseCases.reload(
                currentSession,
                EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.BYPASS_CACHE)
            )
        }
    }

    @Test
    fun handleToolbarStopPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Stop

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.STOP)) }
        verify { sessionUseCases.stopLoading(currentSession) }
    }

    @Test
    fun handleToolbarSettingsPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Settings

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SETTINGS)) }
        verify { navController.navigate(directions, null) }
    }

    @Test
    fun handleToolbarBookmarkPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Bookmark

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARK)) }
        verify { bookmarkTapped(currentSession) }
    }

    @Test
    fun handleToolbarBookmarksPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Bookmarks

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARKS)) }
        verify { navController.navigate(directions, null) }
    }

    @Test
    fun handleToolbarHistoryPress() = runBlockingTest {
        val item = ToolbarMenu.Item.History

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalHistoryFragment()

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.HISTORY)) }
        verify { navController.navigate(directions, null) }
    }

    @Test
    fun handleToolbarRequestDesktopOnPress() = runBlockingTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(true)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_ON)) }
        verify {
            requestDesktopSiteUseCase.invoke(
                true,
                currentSession
            )
        }
    }

    @Test
    fun handleToolbarRequestDesktopOffPress() = runBlockingTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(false)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_OFF)) }
        verify {
            requestDesktopSiteUseCase.invoke(
                false,
                currentSession
            )
        }
    }

    @Test
    fun handleToolbarAddToTopSitesPressed() = runBlockingTest {
        val item = ToolbarMenu.Item.AddToTopSites
        val addPinnedSiteUseCase: TopSitesUseCases.AddPinnedSiteUseCase = mockk(relaxed = true)

        every { topSitesUseCase.addPinnedSites } returns addPinnedSiteUseCase
        every {
            swipeRefreshLayout.context.getString(R.string.snackbar_added_to_top_sites)
        } returns "Added to top sites!"

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { addPinnedSiteUseCase.invoke(currentSession.title, currentSession.url) }
        verify { snackbar.setText("Added to top sites!") }
        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_TOP_SITES)) }
    }

    @Test
    fun handleToolbarAddonsManagerPress() = runBlockingTest {
        val item = ToolbarMenu.Item.AddonsManager

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADDONS_MANAGER)) }
    }

    @Test
    fun handleToolbarAddToHomeScreenPress() = runBlockingTest {
        val item = ToolbarMenu.Item.AddToHomeScreen

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN)) }
    }

    @Test
    fun handleToolbarSharePress() = runBlockingTest {
        val item = ToolbarMenu.Item.Share

        every { currentSession.url } returns "https://mozilla.org"
        every { currentSession.title } returns "Mozilla"

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SHARE)) }
        verify {
            navController.navigate(
                directionsEq(NavGraphDirections.actionGlobalShareFragment(
                    data = arrayOf(ShareData(url = "https://mozilla.org", title = "Mozilla")),
                    showPage = true
                ))
            )
        }
    }

    @Test
    fun handleToolbarFindInPagePress() = runBlockingTest {
        val item = ToolbarMenu.Item.FindInPage

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { findInPageLauncher() }
        verify { metrics.track(Event.FindInPageOpened) }
    }

    @Test
    fun handleToolbarSaveToCollectionPressWhenAtLeastOneCollectionExists() = runBlockingTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollections: List<TabCollection> = mockk(relaxed = true)
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollections

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify {
            metrics.track(
                Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)
            )
        }
        verify {
            metrics.track(
                Event.CollectionSaveButtonPressed(DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER)
            )
        }

        val directions = BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
            saveCollectionStep = SaveCollectionStep.SelectCollection,
            tabIds = arrayOf(currentSession.id),
            selectedTabIds = arrayOf(currentSession.id)
        )
        verify { navController.navigate(directionsEq(directions), null) }
    }

    @Test
    fun handleToolbarSaveToCollectionPressWhenNoCollectionsExists() = runBlockingTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollectionsEmpty: List<TabCollection> = emptyList()
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollectionsEmpty

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)) }
        verify {
            metrics.track(
                Event.CollectionSaveButtonPressed(
                    DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER
                )
            )
        }
        val directions = BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
            saveCollectionStep = SaveCollectionStep.NameCollection,
            tabIds = arrayOf(currentSession.id),
            selectedTabIds = arrayOf(currentSession.id)
        )
        verify { navController.navigate(directionsEq(directions), null) }
    }

    @Test
    fun handleToolbarOpenInFenixPress() = runBlockingTest {
        val controller = createController(scope = this, customTabSession = currentSession)

        val item = ToolbarMenu.Item.OpenInFenix

        every { currentSession.customTabConfig } returns mockk()
        every { activity.startActivity(any()) } just Runs

        controller.handleToolbarItemInteraction(item)

        verify { sessionFeature.release() }
        verify { currentSession.customTabConfig = null }
        verify { sessionManager.select(currentSession) }
        verify { activity.startActivity(openInFenixIntent) }
        verify { activity.finish() }
    }

    @Test
    fun handleToolbarQuitPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Quit
        val testScope = this

        val controller = createController(scope = testScope)

        controller.handleToolbarItemInteraction(item)

        verify { deleteAndQuit(activity, testScope, null) }
    }

    @Test
    fun handleToolbarReaderModeAppearancePress() = runBlockingTest {
        val item = ToolbarMenu.Item.ReaderModeAppearance

        val controller = createController(scope = this)

        controller.handleToolbarItemInteraction(item)

        verify { readerModeController.showControls() }
        verify { metrics.track(Event.ReaderModeAppearanceOpened) }
    }

    @Test
    fun handleToolbarOpenInAppPress() = runBlockingTest {
        val item = ToolbarMenu.Item.OpenInApp

        val controller = createController(scope = this)

        controller.handleToolbarItemInteraction(item)

        verify { settings.openInAppOpened = true }
    }

    private fun createController(
        scope: CoroutineScope,
        activity: HomeActivity = this.activity,
        customTabSession: Session? = null
    ) = DefaultBrowserToolbarMenuController(
        activity = activity,
        navController = navController,
        metrics = metrics,
        settings = settings,
        findInPageLauncher = findInPageLauncher,
        browserAnimator = browserAnimator,
        customTabSession = customTabSession,
        openInFenixIntent = openInFenixIntent,
        scope = scope,
        swipeRefresh = swipeRefreshLayout,
        tabCollectionStorage = tabCollectionStorage,
        bookmarkTapped = bookmarkTapped,
        readerModeController = readerModeController,
        sessionManager = sessionManager,
        sessionFeature = sessionFeatureWrapper
    ).apply {
        ioScope = scope
    }
}
