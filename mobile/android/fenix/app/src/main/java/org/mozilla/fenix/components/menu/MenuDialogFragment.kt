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
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.components.menu.compose.MAIN_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.MainMenu
import org.mozilla.fenix.components.menu.compose.MenuDialogBottomSheet
import org.mozilla.fenix.components.menu.compose.SAVE_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.SaveSubmenu
import org.mozilla.fenix.components.menu.compose.TOOLS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.ToolsSubmenu
import org.mozilla.fenix.components.menu.middleware.MenuDialogMiddleware
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A bottom sheet fragment displaying the menu dialog.
 */
class MenuDialogFragment : BottomSheetDialogFragment() {

    private val args by navArgs<MenuDialogFragmentArgs>()

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

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                MenuDialogBottomSheet(onRequestDismiss = {}) {
                    val browserStore = components.core.store
                    val syncStore = components.backgroundServices.syncStore
                    val bookmarksStorage = components.core.bookmarksStorage
                    val addBookmarkUseCase = components.useCases.bookmarksUseCases.addBookmark
                    val printContentUseCase = components.useCases.sessionUseCases.printContent
                    val selectedTab = browserStore.state.selectedTab

                    val navHostController = rememberNavController()
                    val coroutineScope = rememberCoroutineScope()
                    val store = remember {
                        MenuStore(
                            initialState = MenuState(
                                browserMenuState = if (selectedTab != null) {
                                    BrowserMenuState(selectedTab = selectedTab)
                                } else {
                                    null
                                },
                            ),
                            middleware = listOf(
                                MenuDialogMiddleware(
                                    bookmarksStorage = bookmarksStorage,
                                    addBookmarkUseCase = addBookmarkUseCase,
                                    scope = coroutineScope,
                                ),
                                MenuNavigationMiddleware(
                                    navController = findNavController(),
                                    navHostController = navHostController,
                                    openToBrowser = ::openToBrowser,
                                    scope = coroutineScope,
                                ),
                            ),
                        )
                    }

                    val account by syncStore.observeAsState(initialValue = null) { state -> state.account }
                    val accountState by syncStore.observeAsState(initialValue = NotAuthenticated) { state ->
                        state.accountState
                    }
                    val isBookmarked by store.observeAsState(initialValue = false) { state ->
                        state.browserMenuState != null && state.browserMenuState.bookmarkState.isBookmarked
                    }

                    NavHost(
                        navController = navHostController,
                        startDestination = MAIN_MENU_ROUTE,
                    ) {
                        composable(route = MAIN_MENU_ROUTE) {
                            MainMenu(
                                accessPoint = args.accesspoint,
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
                                onSwitchToDesktopSiteMenuClick = {},
                                onFindInPageMenuClick = {},
                                onToolsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Tools)
                                },
                                onSaveMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Save)
                                },
                                onExtensionsMenuClick = {},
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
                                onCustomizeHomepageMenuClick = {
                                    store.dispatch(MenuAction.Navigate.CustomizeHomepage)
                                },
                                onNewInFirefoxMenuClick = {
                                    store.dispatch(MenuAction.Navigate.ReleaseNotes)
                                },
                            )
                        }

                        composable(route = TOOLS_MENU_ROUTE) {
                            ToolsSubmenu(
                                isReaderViewActive = false,
                                isTranslated = false,
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
                                },
                                onReaderViewMenuClick = {},
                                onTranslatePageMenuClick = {
                                    selectedTab?.let {
                                        store.dispatch(MenuAction.Navigate.Translate)
                                    }
                                },
                                onReviewCheckerMenuClick = {},
                                onPrintMenuClick = {
                                    printContentUseCase()
                                    dismiss()
                                },
                                onShareMenuClick = {
                                    selectedTab?.let {
                                        store.dispatch(MenuAction.Navigate.Share)
                                    }
                                },
                                onOpenInAppMenuClick = {},
                            )
                        }

                        composable(route = SAVE_MENU_ROUTE) {
                            SaveSubmenu(
                                isBookmarked = isBookmarked,
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
                                },
                                onBookmarkPageMenuClick = {
                                    store.dispatch(MenuAction.AddBookmark)
                                },
                                onEditBookmarkButtonClick = {
                                    store.dispatch(MenuAction.Navigate.EditBookmark)
                                },
                                onAddToShortcutsMenuClick = {},
                                onAddToHomeScreenMenuClick = {},
                                onSaveToCollectionMenuClick = {},
                                onSaveAsPDFMenuClick = {},
                            )
                        }
                    }
                }
            }
        }
    }

    private fun openToBrowser(params: BrowserNavigationParams) = runIfFragmentIsAttached {
        val url = params.url ?: params.sumoTopic?.let {
            SupportUtils.getSumoURLForTopic(
                context = requireContext(),
                topic = it,
            )
        }

        url?.let {
            (activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromMenuDialogFragment,
            )
        }
    }
}
