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
import org.mozilla.fenix.components.menu.compose.CUSTOM_TAB_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.CustomTabMenu
import org.mozilla.fenix.components.menu.compose.EXTENSIONS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.ExtensionsSubmenu
import org.mozilla.fenix.components.menu.compose.MAIN_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.MainMenu
import org.mozilla.fenix.components.menu.compose.MenuDialogBottomSheet
import org.mozilla.fenix.components.menu.compose.SAVE_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.SaveSubmenu
import org.mozilla.fenix.components.menu.compose.TOOLS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.ToolsSubmenu
import org.mozilla.fenix.components.menu.middleware.MenuDialogMiddleware
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.middleware.MenuTelemetryMiddleware
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A bottom sheet fragment displaying the menu dialog.
 */
class MenuDialogFragment : BottomSheetDialogFragment() {

    private val args by navArgs<MenuDialogFragmentArgs>()
    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager

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
                    val tabCollectionStorage = components.core.tabCollectionStorage
                    val addBookmarkUseCase = components.useCases.bookmarksUseCases.addBookmark
                    val printContentUseCase = components.useCases.sessionUseCases.printContent
                    val saveToPdfUseCase = components.useCases.sessionUseCases.saveToPdf
                    val selectedTab = browserStore.state.selectedTab
                    val settings = components.settings

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
                                    onDeleteAndQuit = {
                                        deleteAndQuit(
                                            activity = activity as HomeActivity,
                                            coroutineScope = coroutineScope,
                                            snackbar = null,
                                        )
                                    },
                                    scope = coroutineScope,
                                ),
                                MenuNavigationMiddleware(
                                    navController = findNavController(),
                                    navHostController = navHostController,
                                    browsingModeManager = browsingModeManager,
                                    openToBrowser = ::openToBrowser,
                                    scope = coroutineScope,
                                ),
                                MenuTelemetryMiddleware(
                                    accessPoint = args.accesspoint,
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
                        startDestination = when (args.accesspoint) {
                            MenuAccessPoint.Browser,
                            MenuAccessPoint.Home,
                            -> MAIN_MENU_ROUTE

                            MenuAccessPoint.External -> CUSTOM_TAB_MENU_ROUTE
                        },
                    ) {
                        composable(route = MAIN_MENU_ROUTE) {
                            MainMenu(
                                accessPoint = args.accesspoint,
                                account = account,
                                accountState = accountState,
                                isPrivate = browsingModeManager.mode.isPrivate,
                                showQuitMenu = settings.shouldDeleteBrowsingDataOnQuit,
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
                                onNewTabMenuClick = {
                                    store.dispatch(MenuAction.Navigate.NewTab)
                                },
                                onNewPrivateTabMenuClick = {
                                    store.dispatch(MenuAction.Navigate.NewPrivateTab)
                                },
                                onSwitchToDesktopSiteMenuClick = {},
                                onFindInPageMenuClick = {},
                                onToolsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Tools)
                                },
                                onSaveMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Save)
                                },
                                onExtensionsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Extensions)
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
                                onQuitMenuClick = {
                                    store.dispatch(MenuAction.DeleteBrowsingDataAndQuit)
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
                                onSaveToCollectionMenuClick = {
                                    store.dispatch(
                                        MenuAction.Navigate.SaveToCollection(
                                            hasCollection = tabCollectionStorage
                                                .cachedTabCollections.isNotEmpty(),
                                        ),
                                    )
                                },
                                onSaveAsPDFMenuClick = {
                                    saveToPdfUseCase()
                                    dismiss()
                                },
                            )
                        }

                        composable(route = EXTENSIONS_MENU_ROUTE) {
                            ExtensionsSubmenu(
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
                                },
                                onManageExtensionsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.ManageExtensions)
                                },
                                onDiscoverMoreExtensionsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.DiscoverMoreExtensions)
                                },
                            )
                        }

                        composable(route = CUSTOM_TAB_MENU_ROUTE) {
                            CustomTabMenu(
                                onSwitchToDesktopSiteMenuClick = {},
                                onFindInPageMenuClick = {},
                                onOpenInFirefoxMenuClick = {},
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
