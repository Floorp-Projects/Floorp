/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.components.menu.compose.MenuDialog
import org.mozilla.fenix.components.menu.compose.MenuDialogBottomSheet
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A bottom sheet fragment displaying the menu dialog.
 */
class MenuDialogFragment : BottomSheetDialogFragment() {

    private val args by navArgs<MenuDialogFragmentArgs>()

    private val store by lazyStore { viewModelScope ->
        MenuStore(
            initialState = MenuState(),
            middleware = listOf(
                MenuNavigationMiddleware(
                    navController = findNavController(),
                    openSumoTopic = ::openSumoTopic,
                    scope = viewModelScope,
                ),
            ),
        )
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                val bottomSheet = findViewById<View?>(R.id.design_bottom_sheet)
                bottomSheet?.setBackgroundResource(android.R.color.transparent)
                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.peekHeight = resources.displayMetrics.heightPixels
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                MenuDialogBottomSheet(onRequestDismiss = {}) {
                    val syncStore = components.backgroundServices.syncStore
                    val account by syncStore.observeAsState(initialValue = null) { state -> state.account }
                    val accountState by syncStore.observeAsState(initialValue = NotAuthenticated) { state ->
                        state.accountState
                    }

                    MenuDialog(
                        account = account,
                        accountState = accountState,
                        onMozillaAccountButtonClick = {
                            store.dispatch(
                                MenuAction.Navigate.MozillaAccount(
                                    accountState = accountState,
                                    accesspoint = args.accesspoint,
                                ),
                            )
                        },
                        onHelpButtonClick = {
                            store.dispatch(MenuAction.Navigate.Help)
                        },
                        onSettingsButtonClick = {
                            store.dispatch(MenuAction.Navigate.Settings)
                        },
                        onBookmarksMenuClick = {
                            store.dispatch(MenuAction.Navigate.Bookmarks)
                        },
                        onHistoryMenuClick = {
                            store.dispatch(MenuAction.Navigate.History)
                        },
                        onDownloadsMenuClick = {
                            store.dispatch(MenuAction.Navigate.Downloads)
                        },
                        onPasswordsMenuClick = {
                            store.dispatch(MenuAction.Navigate.Passwords)
                        },
                    )
                }
            }
        }
    }

    private fun openSumoTopic(topic: SumoTopic) = runIfFragmentIsAttached {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getSumoURLForTopic(
                context = requireContext(),
                topic = topic,
            ),
            newTab = true,
            from = BrowserDirection.FromMenuDialogFragment,
        )
    }
}
