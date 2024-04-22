/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic

class MenuNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val navController: NavController = mockk(relaxed = true)

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

    private fun createStore(
        openToBrowser: (params: BrowserNavigationParams) -> Unit = {},
    ) = MenuStore(
        initialState = MenuState(),
        middleware = listOf(
            MenuNavigationMiddleware(
                navController = navController,
                openToBrowser = openToBrowser,
                scope = scope,
            ),
        ),
    )
}
