/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.app.Dialog
import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.view.isVisible
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.setFragmentResultListener
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.coroutines.Dispatchers
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.ui.DownloadCancelDialogFragment
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.ComponentTabstray2Binding
import org.mozilla.fenix.databinding.ComponentTabstray3Binding
import org.mozilla.fenix.databinding.ComponentTabstray3FabBinding
import org.mozilla.fenix.databinding.ComponentTabstrayFabBinding
import org.mozilla.fenix.databinding.FragmentTabTrayDialogBinding
import org.mozilla.fenix.databinding.TabsTrayTabCounter2Binding
import org.mozilla.fenix.databinding.TabstrayMultiselectItemsBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.share.ShareFragment
import org.mozilla.fenix.tabstray.browser.SelectionBannerBinding
import org.mozilla.fenix.tabstray.browser.SelectionBannerBinding.VisibilityModifier
import org.mozilla.fenix.tabstray.browser.SelectionHandleBinding
import org.mozilla.fenix.tabstray.browser.TabSorter
import org.mozilla.fenix.tabstray.ext.anchorWithAction
import org.mozilla.fenix.tabstray.ext.bookmarkMessage
import org.mozilla.fenix.tabstray.ext.collectionMessage
import org.mozilla.fenix.tabstray.ext.make
import org.mozilla.fenix.tabstray.ext.showWithTheme
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsIntegration
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.allowUndo
import kotlin.math.max

/**
 * The action or screen that was used to navigate to the Tabs Tray.
 */
enum class TabsTrayAccessPoint {
    None,
    HomeRecentSyncedTab,
}

@Suppress("TooManyFunctions", "LargeClass")
class TabsTrayFragment : AppCompatDialogFragment() {

    @VisibleForTesting internal lateinit var tabsTrayStore: TabsTrayStore
    private lateinit var tabsTrayDialog: TabsTrayDialog
    private lateinit var tabsTrayInteractor: TabsTrayInteractor
    private lateinit var tabsTrayController: DefaultTabsTrayController
    private lateinit var navigationInteractor: DefaultNavigationInteractor

    @VisibleForTesting internal lateinit var trayBehaviorManager: TabSheetBehaviorManager

    private val tabLayoutMediator = ViewBoundFeatureWrapper<TabLayoutMediator>()
    private val tabCounterBinding = ViewBoundFeatureWrapper<TabCounterBinding>()
    private val floatingActionButtonBinding = ViewBoundFeatureWrapper<FloatingActionButtonBinding>()
    private val selectionBannerBinding = ViewBoundFeatureWrapper<SelectionBannerBinding>()
    private val selectionHandleBinding = ViewBoundFeatureWrapper<SelectionHandleBinding>()
    private val tabsTrayCtaBinding = ViewBoundFeatureWrapper<TabsTrayInfoBannerBinding>()
    private val secureTabsTrayBinding = ViewBoundFeatureWrapper<SecureTabsTrayBinding>()
    private val tabsFeature = ViewBoundFeatureWrapper<TabsFeature>()
    private val tabsTrayInactiveTabsOnboardingBinding = ViewBoundFeatureWrapper<TabsTrayInactiveTabsOnboardingBinding>()
    private val syncedTabsIntegration = ViewBoundFeatureWrapper<SyncedTabsIntegration>()

    @VisibleForTesting
    @Suppress("VariableNaming")
    internal var _tabsTrayBinding: ComponentTabstray2Binding? = null
    private val tabsTrayBinding get() = _tabsTrayBinding!!

    @VisibleForTesting
    @Suppress("VariableNaming")
    internal var _tabsTrayDialogBinding: FragmentTabTrayDialogBinding? = null
    private val tabsTrayDialogBinding get() = _tabsTrayDialogBinding!!

    @VisibleForTesting
    @Suppress("VariableNaming")
    internal var _fabButtonBinding: ComponentTabstrayFabBinding? = null
    private val fabButtonBinding get() = _fabButtonBinding!!

    @VisibleForTesting
    @Suppress("VariableNaming")
    internal var _tabsTrayComposeBinding: ComponentTabstray3Binding? = null
    private val tabsTrayComposeBinding get() = _tabsTrayComposeBinding!!

    @Suppress("VariableNaming")
    internal var _fabButtonComposeBinding: ComponentTabstray3FabBinding? = null
    private val fabButtonComposeBinding get() = _fabButtonComposeBinding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.TabTrayDialogStyle)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val args by navArgs<TabsTrayFragmentArgs>()
        args.accessPoint.takeIf { it != TabsTrayAccessPoint.None }?.let {
            TabsTray.accessPoint[it.name.lowercase()].add()
        }
        val initialMode = if (args.enterMultiselect) {
            TabsTrayState.Mode.Select(emptySet())
        } else {
            TabsTrayState.Mode.Normal
        }
        val initialPage = args.page
        val activity = activity as HomeActivity

        tabsTrayStore = StoreProvider.get(this) {
            TabsTrayStore(
                initialState = TabsTrayState(
                    selectedPage = initialPage,
                    mode = initialMode,
                ),
                middlewares = listOf(
                    TabsTrayMiddleware(),
                ),
            )
        }

        navigationInteractor =
            DefaultNavigationInteractor(
                browserStore = requireComponents.core.store,
                navController = findNavController(),
                dismissTabTray = ::dismissTabsTray,
                dismissTabTrayAndNavigateHome = ::dismissTabsTrayAndNavigateHome,
                showCancelledDownloadWarning = ::showCancelledDownloadWarning,
                accountManager = requireComponents.backgroundServices.accountManager,
            )

        tabsTrayController = DefaultTabsTrayController(
            activity = activity,
            appStore = requireComponents.appStore,
            tabsTrayStore = tabsTrayStore,
            browserStore = requireComponents.core.store,
            settings = requireContext().settings(),
            browsingModeManager = activity.browsingModeManager,
            navController = findNavController(),
            navigateToHomeAndDeleteSession = ::navigateToHomeAndDeleteSession,
            navigationInteractor = navigationInteractor,
            profiler = requireComponents.core.engine.profiler,
            tabsUseCases = requireComponents.useCases.tabsUseCases,
            bookmarksUseCase = requireComponents.useCases.bookmarksUseCases,
            ioDispatcher = Dispatchers.IO,
            collectionStorage = requireComponents.core.tabCollectionStorage,
            selectTabPosition = ::selectTabPosition,
            dismissTray = ::dismissTabsTray,
            showUndoSnackbarForTab = ::showUndoSnackbarForTab,
            showCancelledDownloadWarning = ::showCancelledDownloadWarning,
            showCollectionSnackbar = ::showCollectionSnackbar,
            showBookmarkSnackbar = ::showBookmarkSnackbar,
        )

        tabsTrayInteractor = DefaultTabsTrayInteractor(
            controller = tabsTrayController,
        )

        tabsTrayDialog = TabsTrayDialog(requireContext(), theme) { tabsTrayInteractor }
        return tabsTrayDialog
    }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _tabsTrayDialogBinding = FragmentTabTrayDialogBinding.inflate(
            inflater,
            container,
            false,
        )

        if (requireContext().settings().enableTabsTrayToCompose) {
            _tabsTrayComposeBinding = ComponentTabstray3Binding.inflate(
                inflater,
                tabsTrayDialogBinding.root,
                true,
            )

            _fabButtonComposeBinding = ComponentTabstray3FabBinding.inflate(
                inflater,
                tabsTrayDialogBinding.root,
                true,
            )

            tabsTrayComposeBinding.root.setContent {
                FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                    TabsTray(
                        appStore = requireComponents.appStore,
                        browserStore = requireComponents.core.store,
                        tabsTrayStore = tabsTrayStore,
                        displayTabsInGrid = requireContext().settings().gridTabView,
                        isInDebugMode = Config.channel.isDebug ||
                            requireComponents.settings.showSecretDebugMenuThisSession,
                        shouldShowInactiveTabsAutoCloseDialog =
                        requireContext().settings()::shouldShowInactiveTabsAutoCloseDialog,
                        onTabPageClick = { page ->
                            tabsTrayInteractor.onTrayPositionSelected(page.ordinal, false)
                        },
                        onTabClose = { tab ->
                            tabsTrayInteractor.onTabClosed(tab, TABS_TRAY_FEATURE_NAME)
                        },
                        onTabMediaClick = tabsTrayInteractor::onMediaClicked,
                        onTabClick = { tab ->
                            tabsTrayInteractor.onTabSelected(tab, TABS_TRAY_FEATURE_NAME)
                        },
                        onTabMultiSelectClick = { tab ->
                            tabsTrayInteractor.onMultiSelectClicked(tab, TABS_TRAY_FEATURE_NAME)
                        },
                        onTabLongClick = tabsTrayInteractor::onTabLongClicked,
                        onInactiveTabsHeaderClick = tabsTrayInteractor::onInactiveTabsHeaderClicked,
                        onDeleteAllInactiveTabsClick = tabsTrayInteractor::onDeleteAllInactiveTabsClicked,
                        onInactiveTabsAutoCloseDialogShown = {
                            TabsTray.autoCloseSeen.record(NoExtras())
                        },
                        onInactiveTabAutoCloseDialogCloseButtonClick =
                        tabsTrayInteractor::onAutoCloseDialogCloseButtonClicked,
                        onEnableInactiveTabAutoCloseClick = {
                            tabsTrayInteractor.onEnableAutoCloseClicked()
                            showInactiveTabsAutoCloseConfirmationSnackbar()
                        },
                        onInactiveTabClick = tabsTrayInteractor::onInactiveTabClicked,
                        onInactiveTabClose = tabsTrayInteractor::onInactiveTabClosed,
                        onSyncedTabClick = tabsTrayInteractor::onSyncedTabClicked,
                    )
                }
            }

            fabButtonComposeBinding.root.setContent {
                FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                    TabsTrayFab(
                        tabsTrayStore = tabsTrayStore,
                        onNormalTabsFabClicked = tabsTrayInteractor::onNormalTabsFabClicked,
                        onPrivateTabsFabClicked = tabsTrayInteractor::onPrivateTabsFabClicked,
                        onSyncedTabsFabClicked = tabsTrayInteractor::onSyncedTabsFabClicked,
                    )
                }
            }
        } else {
            _tabsTrayBinding = ComponentTabstray2Binding.inflate(
                inflater,
                tabsTrayDialogBinding.root,
                true,
            )
            _fabButtonBinding = ComponentTabstrayFabBinding.inflate(
                inflater,
                tabsTrayDialogBinding.root,
                true,
            )
        }

        return tabsTrayDialogBinding.root
    }

    override fun onStart() {
        super.onStart()
        findPreviousDialogFragment()?.let { dialog ->
            dialog.onAcceptClicked = ::onCancelDownloadWarningAccepted
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _tabsTrayBinding = null
        _tabsTrayDialogBinding = null
        _fabButtonBinding = null
        _tabsTrayComposeBinding = null
        _fabButtonComposeBinding = null
    }

    @Suppress("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        TabsTray.opened.record(NoExtras())

        val rootView = if (requireContext().settings().enableTabsTrayToCompose) {
            tabsTrayComposeBinding.root
        } else {
            tabsTrayBinding.tabWrapper
        }

        val newTabFab = if (requireContext().settings().enableTabsTrayToCompose) {
            fabButtonComposeBinding.root
        } else {
            fabButtonBinding.newTabButton
        }

        val behavior = BottomSheetBehavior.from(rootView).apply {
            addBottomSheetCallback(
                TraySheetBehaviorCallback(
                    this,
                    navigationInteractor,
                    tabsTrayDialog,
                    newTabFab,
                ),
            )
            skipCollapsed = true
        }

        trayBehaviorManager = TabSheetBehaviorManager(
            behavior = behavior,
            orientation = resources.configuration.orientation,
            maxNumberOfTabs = max(
                requireContext().components.core.store.state.normalTabs.size,
                requireContext().components.core.store.state.privateTabs.size,
            ),
            numberForExpandingTray = if (requireContext().settings().gridTabView) {
                EXPAND_AT_GRID_SIZE
            } else {
                EXPAND_AT_LIST_SIZE
            },
            displayMetrics = requireContext().resources.displayMetrics,
        )

        setupBackgroundDismissalListener {
            TabsTray.closed.record(NoExtras())
            dismissAllowingStateLoss()
        }

        if (!requireContext().settings().enableTabsTrayToCompose) {
            val activity = activity as HomeActivity

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
                fabButtonBinding.newTabButton.accessibilityTraversalAfter =
                    tabsTrayBinding.tabLayout.id
            }

            setupMenu(navigationInteractor)
            setupPager(
                context = view.context,
                lifecycleOwner = viewLifecycleOwner,
                store = tabsTrayStore,
                trayInteractor = tabsTrayInteractor,
            )

            tabsTrayCtaBinding.set(
                feature = TabsTrayInfoBannerBinding(
                    context = view.context,
                    store = requireComponents.core.store,
                    infoBannerView = tabsTrayBinding.infoBanner,
                    settings = requireComponents.settings,
                    navigationInteractor = navigationInteractor,
                ),
                owner = this,
                view = view,
            )

            tabLayoutMediator.set(
                feature = TabLayoutMediator(
                    tabLayout = tabsTrayBinding.tabLayout,
                    tabPager = tabsTrayBinding.tabsTray,
                    interactor = tabsTrayInteractor,
                    browsingModeManager = activity.browsingModeManager,
                    tabsTrayStore = tabsTrayStore,
                ),
                owner = this,
                view = view,
            )

            val tabsTrayTabCounter2Binding = TabsTrayTabCounter2Binding.bind(
                tabsTrayBinding.tabLayout,
            )

            tabCounterBinding.set(
                feature = TabCounterBinding(
                    store = requireComponents.core.store,
                    counter = tabsTrayTabCounter2Binding.tabCounter,
                ),
                owner = this,
                view = view,
            )

            floatingActionButtonBinding.set(
                feature = FloatingActionButtonBinding(
                    store = tabsTrayStore,
                    actionButton = fabButtonBinding.newTabButton,
                    interactor = tabsTrayInteractor,
                ),
                owner = this,
                view = view,
            )

            val tabsTrayMultiselectItemsBinding = TabstrayMultiselectItemsBinding.bind(
                tabsTrayBinding.root,
            )

            selectionBannerBinding.set(
                feature = SelectionBannerBinding(
                    context = requireContext(),
                    binding = tabsTrayBinding,
                    store = tabsTrayStore,
                    interactor = tabsTrayInteractor,
                    backgroundView = tabsTrayBinding.topBar,
                    showOnSelectViews = VisibilityModifier(
                        tabsTrayMultiselectItemsBinding.collectMultiSelect,
                        tabsTrayMultiselectItemsBinding.shareMultiSelect,
                        tabsTrayMultiselectItemsBinding.menuMultiSelect,
                        tabsTrayBinding.multiselectTitle,
                        tabsTrayBinding.exitMultiSelect,
                    ),
                    showOnNormalViews = VisibilityModifier(
                        tabsTrayBinding.tabLayout,
                        tabsTrayBinding.tabTrayOverflow,
                        fabButtonBinding.newTabButton,
                    ),
                ),
                owner = this,
                view = view,
            )

            selectionHandleBinding.set(
                feature = SelectionHandleBinding(
                    store = tabsTrayStore,
                    handle = tabsTrayBinding.handle,
                    containerLayout = tabsTrayBinding.tabWrapper,
                ),
                owner = this,
                view = view,
            )

            tabsTrayInactiveTabsOnboardingBinding.set(
                feature = TabsTrayInactiveTabsOnboardingBinding(
                    context = requireContext(),
                    store = requireComponents.core.store,
                    tabsTrayBinding = tabsTrayBinding,
                    settings = requireComponents.settings,
                    navigationInteractor = navigationInteractor,
                ),
                owner = this,
                view = view,
            )
        }

        tabsFeature.set(
            feature = TabsFeature(
                tabsTray = TabSorter(
                    requireContext().settings(),
                    tabsTrayStore,
                ),
                store = requireContext().components.core.store,
            ),
            owner = this,
            view = view,
        )

        secureTabsTrayBinding.set(
            feature = SecureTabsTrayBinding(
                store = tabsTrayStore,
                settings = requireComponents.settings,
                fragment = this,
                dialog = dialog as TabsTrayDialog,
            ),
            owner = this,
            view = view,
        )

        syncedTabsIntegration.set(
            feature = SyncedTabsIntegration(
                store = tabsTrayStore,
                context = requireContext(),
                navController = findNavController(),
                storage = requireComponents.backgroundServices.syncedTabsStorage,
                accountManager = requireComponents.backgroundServices.accountManager,
                lifecycleOwner = this,
            ),
            owner = this,
            view = view,
        )

        setFragmentResultListener(ShareFragment.RESULT_KEY) { _, _ ->
            dismissTabsTray()
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        if (!requireContext().settings().enableTabsTrayToCompose) {
            trayBehaviorManager.updateDependingOnOrientation(newConfig.orientation)

            if (requireContext().settings().gridTabView) {
                tabsTrayBinding.tabsTray.adapter?.notifyDataSetChanged()
            }
        }
    }

    @VisibleForTesting
    internal fun onCancelDownloadWarningAccepted(tabId: String?, source: String?) {
        if (tabId != null) {
            tabsTrayInteractor.onDeletePrivateTabWarningAccepted(tabId, source)
        } else {
            navigationInteractor.onCloseAllPrivateTabsWarningConfirmed(private = true)
        }
    }

    @VisibleForTesting
    internal fun showCancelledDownloadWarning(downloadCount: Int, tabId: String?, source: String?) {
        val dialog = DownloadCancelDialogFragment.newInstance(
            downloadCount = downloadCount,
            tabId = tabId,
            source = source,
            promptStyling = DownloadCancelDialogFragment.PromptStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true,
                positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                    R.attr.accent,
                    requireContext(),
                ),
                positiveButtonTextColor = ThemeManager.resolveAttribute(
                    R.attr.textOnColorPrimary,
                    requireContext(),
                ),
                positiveButtonRadius = (resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat(),
            ),

            onPositiveButtonClicked = ::onCancelDownloadWarningAccepted,
        )
        dialog.show(parentFragmentManager, DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG)
    }

    @VisibleForTesting
    internal fun showUndoSnackbarForTab(isPrivate: Boolean) {
        val snackbarMessage =
            when (isPrivate) {
                true -> getString(R.string.snackbar_private_tab_closed)
                false -> getString(R.string.snackbar_tab_closed)
            }
        val pagePosition = if (isPrivate) Page.PrivateTabs.ordinal else Page.NormalTabs.ordinal

        lifecycleScope.allowUndo(
            view = requireView(),
            message = snackbarMessage,
            undoActionTitle = getString(R.string.snackbar_deleted_undo),
            onCancel = {
                requireComponents.useCases.tabsUseCases.undo.invoke()

                if (requireContext().settings().enableTabsTrayToCompose) {
                    tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(pagePosition)))
                } else {
                    tabLayoutMediator.withFeature {
                        it.selectTabAtPosition(pagePosition)
                    }
                }
            },
            operation = { },
            elevation = ELEVATION,
            anchorView = when {
                requireContext().settings().enableTabsTrayToCompose -> fabButtonComposeBinding.root
                fabButtonBinding.newTabButton.isVisible -> fabButtonBinding.newTabButton
                else -> null
            },
        )
    }

    @VisibleForTesting
    @Suppress("LongParameterList")
    internal fun setupPager(
        context: Context,
        lifecycleOwner: LifecycleOwner,
        store: TabsTrayStore,
        trayInteractor: TabsTrayInteractor,
    ) {
        tabsTrayBinding.tabsTray.apply {
            adapter = TrayPagerAdapter(
                context = context,
                lifecycleOwner = lifecycleOwner,
                tabsTrayStore = store,
                interactor = trayInteractor,
                browserStore = requireComponents.core.store,
                appStore = requireComponents.appStore,
            )
            isUserInputEnabled = false
        }
    }

    @VisibleForTesting
    internal fun setupMenu(navigationInteractor: NavigationInteractor) {
        tabsTrayBinding.tabTrayOverflow.setOnClickListener { anchor ->

            TabsTray.menuOpened.record(NoExtras())

            val menu = getTrayMenu(
                context = requireContext(),
                browserStore = requireComponents.core.store,
                tabsTrayStore = tabsTrayStore,
                tabLayout = tabsTrayBinding.tabLayout,
                navigationInteractor = navigationInteractor,
            ).build()

            menu.showWithTheme(anchor)
        }
    }

    @VisibleForTesting
    internal fun getTrayMenu(
        context: Context,
        browserStore: BrowserStore,
        tabsTrayStore: TabsTrayStore,
        tabLayout: TabLayout,
        navigationInteractor: NavigationInteractor,
    ) = MenuIntegration(context, browserStore, tabsTrayStore, tabLayout, navigationInteractor)

    @VisibleForTesting
    internal fun setupBackgroundDismissalListener(block: (View) -> Unit) {
        tabsTrayDialogBinding.tabLayout.setOnClickListener(block)
        if (!requireContext().settings().enableTabsTrayToCompose) {
            tabsTrayBinding.handle.setOnClickListener(block)
        }
    }

    @VisibleForTesting
    internal fun dismissTabsTrayAndNavigateHome(sessionId: String) {
        navigateToHomeAndDeleteSession(sessionId)
        dismissTabsTray()
    }

    internal val homeViewModel: HomeScreenViewModel by activityViewModels()

    @VisibleForTesting
    internal fun navigateToHomeAndDeleteSession(sessionId: String) {
        homeViewModel.sessionToDelete = sessionId
        val directions = NavGraphDirections.actionGlobalHome()
        findNavController().navigate(directions)
    }

    @VisibleForTesting
    internal fun selectTabPosition(position: Int, smoothScroll: Boolean) {
        if (!requireContext().settings().enableTabsTrayToCompose) {
            tabsTrayBinding.tabsTray.setCurrentItem(position, smoothScroll)
            tabsTrayBinding.tabLayout.getTabAt(position)?.select()
        }
    }

    @VisibleForTesting
    internal fun dismissTabsTray() {
        // This should always be the last thing we do because nothing (e.g. telemetry)
        // is guaranteed after that.
        dismissAllowingStateLoss()
    }

    @VisibleForTesting
    internal fun showCollectionSnackbar(
        tabSize: Int,
        isNewCollection: Boolean = false,
    ) {
        runIfFragmentIsAttached {
            FenixSnackbar
                .make(requireView())
                .collectionMessage(tabSize, isNewCollection)
                .anchorWithAction(getSnackbarAnchor()) {
                    findNavController().navigate(
                        TabsTrayFragmentDirections.actionGlobalHome(
                            focusOnAddressBar = false,
                            scrollToCollection = true,
                        ),
                    )
                    dismissTabsTray()
                }.show()
        }
    }

    @VisibleForTesting
    internal fun showBookmarkSnackbar(
        tabSize: Int,
    ) {
        FenixSnackbar
            .make(requireView())
            .bookmarkMessage(tabSize)
            .anchorWithAction(getSnackbarAnchor()) {
                findNavController().navigate(
                    TabsTrayFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
                )
                dismissTabsTray()
            }
            .show()
    }

    @Suppress("MaxLineLength")
    private fun findPreviousDialogFragment(): DownloadCancelDialogFragment? {
        return parentFragmentManager.findFragmentByTag(DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG) as? DownloadCancelDialogFragment
    }

    private fun getSnackbarAnchor(): View? {
        return if (requireComponents.settings.accessibilityServicesEnabled) {
            null
        } else {
            fabButtonBinding.newTabButton
        }
    }

    private fun showInactiveTabsAutoCloseConfirmationSnackbar() {
        val text = getString(R.string.inactive_tabs_auto_close_message_snackbar)
        val snackbar = FenixSnackbar.make(
            view = tabsTrayComposeBinding.root,
            duration = FenixSnackbar.LENGTH_SHORT,
            isDisplayedWithBrowserToolbar = true,
        ).setText(text)
        snackbar.view.elevation = ELEVATION
        snackbar.show()
    }

    companion object {
        private const val DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG = "DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG"

        // Minimum number of list items for which to show the tabs tray as expanded.
        const val EXPAND_AT_LIST_SIZE = 4

        // Minimum number of grid items for which to show the tabs tray as expanded.
        private const val EXPAND_AT_GRID_SIZE = 3

        // Elevation for undo toasts
        @VisibleForTesting
        internal const val ELEVATION = 80f

        private const val TABS_TRAY_FEATURE_NAME = "Tabs tray"
    }
}
