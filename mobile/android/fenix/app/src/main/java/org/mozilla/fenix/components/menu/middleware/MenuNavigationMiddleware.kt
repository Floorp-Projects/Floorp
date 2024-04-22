/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.middleware

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.Authenticating
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.MenuDialogFragmentDirections
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.components.menu.toFenixFxAEntryPoint
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.SupportUtils.SumoTopic

/**
 * [Middleware] implementation for handling navigating events based on [MenuAction]s that are
 * dispatched to the [MenuStore].
 *
 * @param navController [NavController] used for navigation.
 * @param openSumoTopic Callback to open the provided [SumoTopic] in a new browser tab.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
class MenuNavigationMiddleware(
    private val navController: NavController,
    private val openSumoTopic: (topic: SumoTopic) -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<MenuState, MenuAction> {

    override fun invoke(
        context: MiddlewareContext<MenuState, MenuAction>,
        next: (MenuAction) -> Unit,
        action: MenuAction,
    ) {
        next(action)

        scope.launch {
            when (action) {
                is MenuAction.Navigate.MozillaAccount -> {
                    when (action.accountState) {
                        Authenticated -> navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalAccountSettingsFragment(),
                        )

                        AuthenticationProblem -> navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalAccountProblemFragment(
                                entrypoint = action.accesspoint.toFenixFxAEntryPoint(),
                            ),
                        )

                        is Authenticating, NotAuthenticated -> navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalTurnOnSync(
                                entrypoint = action.accesspoint.toFenixFxAEntryPoint(),
                            ),
                        )
                    }
                }

                is MenuAction.Navigate.Help -> openSumoTopic(SumoTopic.HELP)

                is MenuAction.Navigate.Settings -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalSettingsFragment(),
                )

                is MenuAction.Navigate.Bookmarks -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
                )

                is MenuAction.Navigate.History -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalHistoryFragment(),
                )

                is MenuAction.Navigate.Downloads -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalDownloadsFragment(),
                )

                is MenuAction.Navigate.Passwords -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalSavedLoginsAuthFragment(),
                )

                else -> Unit
            }
        }
    }
}
