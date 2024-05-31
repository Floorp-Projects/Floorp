/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import androidx.navigation.NavController
import androidx.navigation.NavHostController
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.SimpleBrowsingModeManager
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.components.menu.compose.EXTENSIONS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.SAVE_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.TOOLS_MENU_ROUTE
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.store.BookmarkState
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.AMO_HOMEPAGE_FOR_ANDROID
import org.mozilla.fenix.settings.SupportUtils.SumoTopic

class MenuNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val navController: NavController = mockk(relaxed = true)
    private val navHostController: NavHostController = mockk(relaxed = true)

    @Test
    fun `GIVEN account state is authenticated WHEN navigate to Mozilla account action is dispatched THEN dispatch navigate action to Mozilla account settings`() = runTest {
        val store = createStore()
        val accountState = Authenticated
        val accesspoint = MenuAccessPoint.Home

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = accountState,
                accesspoint = accesspoint,
            ),
        ).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalAccountSettingsFragment(),
            )
        }
    }

    @Test
    fun `GIVEN account state is authentication problem WHEN navigate to Mozilla account action is dispatched THEN dispatch navigate action to Mozilla account problem`() = runTest {
        val store = createStore()
        val accountState = AuthenticationProblem
        val accesspoint = MenuAccessPoint.Home

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = accountState,
                accesspoint = accesspoint,
            ),
        ).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalAccountProblemFragment(
                    entrypoint = FenixFxAEntryPoint.BrowserToolbar,
                ),
            )
        }
    }

    @Test
    fun `GIVEN account state is not authenticated WHEN navigate to Mozilla account action is dispatched THEN dispatch navigate action to turn on sync`() = runTest {
        val store = createStore()
        val accountState = NotAuthenticated
        val accesspoint = MenuAccessPoint.Home

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = accountState,
                accesspoint = accesspoint,
            ),
        ).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalTurnOnSync(
                    entrypoint = FenixFxAEntryPoint.HomeMenu,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to settings action is dispatched THEN navigate to settings`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalSettingsFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to help action is dispatched THEN navigate to SUMO Help topic`() = runTest {
        var params: BrowserNavigationParams? = null
        val store = createStore(
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.Help).join()

        assertEquals(SumoTopic.HELP, params?.sumoTopic)
    }

    @Test
    fun `WHEN navigate to bookmarks action is dispatched THEN navigate to bookmarks`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
            )
        }
    }

    @Test
    fun `WHEN navigate to history action is dispatched THEN navigate to history`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalHistoryFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to downloads action is dispatched THEN navigate to downloads`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalDownloadsFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to passwords action is dispatched THEN navigate to passwords`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Settings).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalSavedLoginsAuthFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to customize homepage action is dispatched THEN navigate to homepage settings`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.CustomizeHomepage).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalHomeSettingsFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to release notes action is dispatched THEN navigate to SUMO topic`() = runTest {
        var params: BrowserNavigationParams? = null
        val store = createStore(
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.ReleaseNotes).join()

        assertEquals(SupportUtils.WHATS_NEW_URL, params?.url)
    }

    @Test
    fun `WHEN navigate to tools action is dispatched THEN navigate to tools submenu route`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Tools).join()

        verify {
            navHostController.navigate(route = TOOLS_MENU_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate to save action is dispatched THEN navigate to save submenu route`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Save).join()

        verify {
            navHostController.navigate(route = SAVE_MENU_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate to extensions action is dispatched THEN navigate to extensions submenu route`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Extensions).join()

        verify {
            navHostController.navigate(route = EXTENSIONS_MENU_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate back action is dispatched THEN pop back stack`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.Back).join()

        verify { navHostController.popBackStack() }
    }

    @Test
    fun `GIVEN there are existing tab collections WHEN navigate to save to collection action is dispatched THEN navigate to select collection creation`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.SaveToCollection(hasCollection = true)).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalCollectionCreationFragment(
                    tabIds = arrayOf(tab.id),
                    selectedTabIds = arrayOf(tab.id),
                    saveCollectionStep = SaveCollectionStep.SelectCollection,
                ),
            )
        }
    }

    @Test
    fun `GIVEN there are no existing tab collections WHEN navigate to save to collection action is dispatched THEN navigate to new collection creation`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.SaveToCollection(hasCollection = false)).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalCollectionCreationFragment(
                    tabIds = arrayOf(tab.id),
                    selectedTabIds = arrayOf(tab.id),
                    saveCollectionStep = SaveCollectionStep.NameCollection,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to edit bookmark action is dispatched THEN navigate to bookmark edit fragment`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                    bookmarkState = BookmarkState(
                        guid = BookmarkRoot.Mobile.id,
                        isBookmarked = true,
                    ),
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.EditBookmark).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalBookmarkEditFragment(
                    guidToEdit = BookmarkRoot.Mobile.id,
                    requiresSnackbarPaddingForToolbar = true,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to translate action is dispatched THEN navigate to translation dialog`() = runTest {
        val tab = createTab(url = "https://www.mozilla.org")
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Translate).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionMenuDialogFragmentToTranslationsDialogFragment(),
            )
        }
    }

    @Test
    fun `GIVEN reader view is active WHEN navigate to share action is dispatched THEN navigate to share sheet`() = runTest {
        val title = "Mozilla"
        val readerUrl = "moz-extension://1234"
        val activeUrl = "https://mozilla.org"
        val readerTab = createTab(
            url = readerUrl,
            readerState = ReaderState(active = true, activeUrl = activeUrl),
            title = title,
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = readerTab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Share).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalShareFragment(
                    sessionId = readerTab.id,
                    data = arrayOf(
                        ShareData(
                            url = activeUrl,
                            title = title,
                        ),
                    ),
                    showPage = true,
                ),
            )
        }
    }

    @Test
    fun `GIVEN reader view is inactive WHEN navigate to share action is dispatched THEN navigate to share sheet`() = runTest {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val tab = createTab(
            url = url,
            title = title,
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = BrowserMenuState(
                    selectedTab = tab,
                ),
            ),
        )

        store.dispatch(MenuAction.Navigate.Share).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalShareFragment(
                    sessionId = tab.id,
                    data = arrayOf(
                        ShareData(
                            url = url,
                            title = title,
                        ),
                    ),
                    showPage = true,
                ),
            )
        }
    }

    @Test
    fun `WHEN navigate to manage extensions action is dispatched THEN navigate to the extensions management`() = runTest {
        val store = createStore()
        store.dispatch(MenuAction.Navigate.ManageExtensions).join()

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalAddonsManagementFragment(),
            )
        }
    }

    @Test
    fun `WHEN navigate to discover more extensions action is dispatched THEN navigate to the AMO page`() = runTest {
        var params: BrowserNavigationParams? = null
        val store = createStore(
            openToBrowser = {
                params = it
            },
        )

        store.dispatch(MenuAction.Navigate.DiscoverMoreExtensions).join()

        assertEquals(AMO_HOMEPAGE_FOR_ANDROID, params?.url)
    }

    @Test
    fun `WHEN navigate to new tab action is dispatched THEN navigate to the home screen`() = runTest {
        val browsingModeManager = SimpleBrowsingModeManager(BrowsingMode.Private)
        val store = createStore(
            browsingModeManager = browsingModeManager,
        )
        store.dispatch(MenuAction.Navigate.NewTab).join()

        assertEquals(BrowsingMode.Normal, browsingModeManager.mode)

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
        }
    }

    @Test
    fun `WHEN navigate to new private tab action is dispatched THEN navigate to the home screen in private mode`() = runTest {
        val browsingModeManager = SimpleBrowsingModeManager(BrowsingMode.Normal)
        val store = createStore(
            browsingModeManager = browsingModeManager,
        )
        store.dispatch(MenuAction.Navigate.NewPrivateTab).join()

        assertEquals(BrowsingMode.Private, browsingModeManager.mode)

        verify {
            navController.nav(
                R.id.menuDialogFragment,
                MenuDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
        }
    }

    private fun createStore(
        menuState: MenuState = MenuState(),
        browsingModeManager: BrowsingModeManager = mockk(relaxed = true),
        openToBrowser: (params: BrowserNavigationParams) -> Unit = {},
    ) = MenuStore(
        initialState = menuState,
        middleware = listOf(
            MenuNavigationMiddleware(
                navController = navController,
                navHostController = navHostController,
                browsingModeManager = browsingModeManager,
                openToBrowser = openToBrowser,
                scope = scope,
            ),
        ),
    )
}
