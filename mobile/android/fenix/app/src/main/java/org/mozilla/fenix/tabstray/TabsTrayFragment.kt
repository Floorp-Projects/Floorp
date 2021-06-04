/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.android.synthetic.main.component_tabstray2.*
import kotlinx.android.synthetic.main.component_tabstray2.view.*
import kotlinx.android.synthetic.main.component_tabstray2.view.tab_tray_overflow
import kotlinx.android.synthetic.main.component_tabstray2.view.tab_wrapper
import kotlinx.android.synthetic.main.component_tabstray_fab.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.*
import kotlinx.android.synthetic.main.tabs_tray_tab_counter2.*
import kotlinx.android.synthetic.main.tabstray_multiselect_items.*
import kotlinx.coroutines.Dispatchers
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.navigateBlockingForAsyncNavGraph
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.DefaultBrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.SelectionHandleBinding
import org.mozilla.fenix.tabstray.browser.SelectionBannerBinding
import org.mozilla.fenix.tabstray.browser.SelectionBannerBinding.VisibilityModifier
import org.mozilla.fenix.tabstray.ext.showWithTheme
import org.mozilla.fenix.tabstray.ext.anchorWithAction
import org.mozilla.fenix.utils.allowUndo
import kotlin.math.max
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.tabstray.ext.make
import org.mozilla.fenix.tabstray.ext.orDefault
import org.mozilla.fenix.tabstray.ext.message

@Suppress("TooManyFunctions", "LargeClass")
class TabsTrayFragment : AppCompatDialogFragment() {
    private var fabView: View? = null
    @VisibleForTesting internal lateinit var tabsTrayStore: TabsTrayStore
    private lateinit var browserTrayInteractor: BrowserTrayInteractor
    private lateinit var tabsTrayInteractor: TabsTrayInteractor
    private lateinit var tabsTrayController: DefaultTabsTrayController
    private lateinit var behavior: BottomSheetBehavior<ConstraintLayout>

    private val tabLayoutMediator = ViewBoundFeatureWrapper<TabLayoutMediator>()
    private val tabCounterBinding = ViewBoundFeatureWrapper<TabCounterBinding>()
    private val floatingActionButtonBinding = ViewBoundFeatureWrapper<FloatingActionButtonBinding>()
    private val newTabButtonBinding = ViewBoundFeatureWrapper<AccessibleNewTabButtonBinding>()
    private val selectionBannerBinding = ViewBoundFeatureWrapper<SelectionBannerBinding>()
    private val selectionHandleBinding = ViewBoundFeatureWrapper<SelectionHandleBinding>()
    private val tabsTrayCtaBinding = ViewBoundFeatureWrapper<TabsTrayInfoBannerBinding>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.TabTrayDialogStyle)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?) =
        TabsTrayDialog(requireContext(), theme) { browserTrayInteractor }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val containerView = inflater.inflate(R.layout.fragment_tab_tray_dialog, container, false)
        val view: View = LayoutInflater.from(containerView.context)
            .inflate(R.layout.component_tabstray2, containerView as ViewGroup, true)

        behavior = BottomSheetBehavior.from(view.tab_wrapper)

        tabsTrayStore = StoreProvider.get(this) { TabsTrayStore() }

        fabView = LayoutInflater.from(containerView.context)
            .inflate(R.layout.component_tabstray_fab, containerView, true)

        return containerView
    }

    @Suppress("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val activity = activity as HomeActivity

        requireComponents.analytics.metrics.track(Event.TabsTrayOpened)

        val navigationInteractor =
            DefaultNavigationInteractor(
                context = requireContext(),
                activity = activity,
                tabsTrayStore = tabsTrayStore,
                browserStore = requireComponents.core.store,
                navController = findNavController(),
                metrics = requireComponents.analytics.metrics,
                dismissTabTray = ::dismissTabsTray,
                dismissTabTrayAndNavigateHome = ::dismissTabsTrayAndNavigateHome,
                bookmarksUseCase = requireComponents.useCases.bookmarksUseCases,
                collectionStorage = requireComponents.core.tabCollectionStorage,
                showCollectionSnackbar = ::showCollectionSnackbar,
                accountManager = requireComponents.backgroundServices.accountManager,
                ioDispatcher = Dispatchers.IO
            )

        tabsTrayController = DefaultTabsTrayController(
            trayStore = tabsTrayStore,
            browserStore = requireComponents.core.store,
            browsingModeManager = activity.browsingModeManager,
            navController = findNavController(),
            navigateToHomeAndDeleteSession = ::navigateToHomeAndDeleteSession,
            navigationInteractor = navigationInteractor,
            profiler = requireComponents.core.engine.profiler,
            metrics = requireComponents.analytics.metrics,
            tabsUseCases = requireComponents.useCases.tabsUseCases,
            selectTabPosition = ::selectTabPosition,
            dismissTray = ::dismissTabsTray,
            showUndoSnackbarForTab = ::showUndoSnackbarForTab
        )

        tabsTrayInteractor = DefaultTabsTrayInteractor(tabsTrayController)

        browserTrayInteractor = DefaultBrowserTrayInteractor(
            tabsTrayStore,
            tabsTrayInteractor,
            tabsTrayController,
            requireComponents.useCases.tabsUseCases.selectTab,
            requireComponents.settings,
            requireComponents.analytics.metrics
        )

        setupMenu(view, navigationInteractor)
        setupPager(
            view.context,
            tabsTrayStore,
            tabsTrayInteractor,
            browserTrayInteractor,
            navigationInteractor
        )

        setupBackgroundDismissalListener {
            requireComponents.analytics.metrics.track(Event.TabsTrayClosed)
            dismissAllowingStateLoss()
        }

        behavior.setUpTrayBehavior(
            isLandscape = requireContext().resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE,
            maxNumberOfTabs = max(
                requireContext().components.core.store.state.normalTabs.size,
                requireContext().components.core.store.state.privateTabs.size
            ),
            numberForExpandingTray = if (requireContext().settings().gridTabView) {
                EXPAND_AT_GRID_SIZE
            } else {
                EXPAND_AT_LIST_SIZE
            },
            navigationInteractor = navigationInteractor
        )

        tabsTrayCtaBinding.set(
            feature = TabsTrayInfoBannerBinding(
                context = view.context,
                store = requireComponents.core.store,
                infoBannerView = view.info_banner,
                settings = requireComponents.settings,
                navigationInteractor = navigationInteractor,
                metrics = requireComponents.analytics.metrics
            ),
            owner = this,
            view = view
        )

        tabLayoutMediator.set(
            feature = TabLayoutMediator(
                tabLayout = tab_layout,
                interactor = tabsTrayInteractor,
                browsingModeManager = activity.browsingModeManager,
                tabsTrayStore = tabsTrayStore,
                metrics = requireComponents.analytics.metrics
            ), owner = this,
            view = view
        )

        tabCounterBinding.set(
            feature = TabCounterBinding(
                store = requireComponents.core.store,
                counter = tab_counter
            ),
            owner = this,
            view = view
        )

        floatingActionButtonBinding.set(
            feature = FloatingActionButtonBinding(
                store = tabsTrayStore,
                settings = requireComponents.settings,
                actionButton = new_tab_button,
                browserTrayInteractor = browserTrayInteractor
            ),
            owner = this,
            view = view
        )

        newTabButtonBinding.set(
            feature = AccessibleNewTabButtonBinding(
                store = tabsTrayStore,
                settings = requireComponents.settings,
                newTabButton = tab_tray_new_tab,
                browserTrayInteractor = browserTrayInteractor
            ),
            owner = this,
            view = view
        )

        selectionBannerBinding.set(
            feature = SelectionBannerBinding(
                context = requireContext(),
                store = tabsTrayStore,
                navInteractor = navigationInteractor,
                tabsTrayInteractor = tabsTrayInteractor,
                containerView = view,
                backgroundView = topBar,
                showOnSelectViews = VisibilityModifier(
                    collect_multi_select,
                    share_multi_select,
                    menu_multi_select,
                    multiselect_title,
                    exit_multi_select
                ),
                showOnNormalViews = VisibilityModifier(
                    tab_layout,
                    tab_tray_overflow,
                    new_tab_button
                )
            ),
            owner = this,
            view = view
        )

        selectionHandleBinding.set(
            feature = SelectionHandleBinding(
                store = tabsTrayStore,
                handle = handle,
                containerLayout = tab_wrapper
            ),
            owner = this,
            view = view
        )
    }

    @VisibleForTesting
    internal fun showUndoSnackbarForTab(isPrivate: Boolean) {
        val snackbarMessage =
            when (isPrivate) {
                true -> getString(R.string.snackbar_private_tab_closed)
                false -> getString(R.string.snackbar_tab_closed)
            }

        lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo),
            {
                requireComponents.useCases.tabsUseCases.undo.invoke()
                tabLayoutMediator.withFeature { it.selectTabAtPosition(
                    if (isPrivate) Page.PrivateTabs.ordinal else Page.NormalTabs.ordinal
                ) }
            },
            operation = { },
            elevation = ELEVATION,
            anchorView = if (new_tab_button.isVisible) new_tab_button else null
        )
    }

    @VisibleForTesting
    internal fun setupPager(
        context: Context,
        store: TabsTrayStore,
        trayInteractor: TabsTrayInteractor,
        browserInteractor: BrowserTrayInteractor,
        navigationInteractor: NavigationInteractor
    ) {
        tabsTray.apply {
            adapter = TrayPagerAdapter(
                context,
                store,
                browserInteractor,
                navigationInteractor,
                trayInteractor,
                requireComponents.core.store
            )
            isUserInputEnabled = false
        }
    }

    @VisibleForTesting
    internal fun setupMenu(view: View, navigationInteractor: NavigationInteractor) {
        view.tab_tray_overflow.setOnClickListener { anchor ->

            requireComponents.analytics.metrics.track(Event.TabsTrayMenuOpened)

            val menu = getTrayMenu(
                context = requireContext(),
                browserStore = requireComponents.core.store,
                tabsTrayStore = tabsTrayStore,
                tabLayout = tab_layout,
                navigationInteractor = navigationInteractor
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
        navigationInteractor: NavigationInteractor
    ) = MenuIntegration(context, browserStore, tabsTrayStore, tabLayout, navigationInteractor)

    @VisibleForTesting
    internal fun setupBackgroundDismissalListener(block: (View) -> Unit) {
        tabLayout.setOnClickListener(block)
        handle.setOnClickListener(block)
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
        findNavController().navigateBlockingForAsyncNavGraph(directions)
    }

    @VisibleForTesting
    internal fun selectTabPosition(position: Int, smoothScroll: Boolean) {
        tabsTray.setCurrentItem(position, smoothScroll)
        tab_layout.getTabAt(position)?.select()
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
        collectionToSelect: Long?
    ) {
        val anchor = if (requireComponents.settings.accessibilityServicesEnabled) {
            null
        } else {
            new_tab_button
        }

        FenixSnackbar
            .make(requireView())
            .message(tabSize, isNewCollection)
            .anchorWithAction(anchor) {
                findNavController().navigateBlockingForAsyncNavGraph(
                    TabsTrayFragmentDirections.actionGlobalHome(
                        focusOnAddressBar = false,
                        focusOnCollection = collectionToSelect.orDefault()
                    )
                )
                dismissTabsTray()
            }.show()
    }

    companion object {
        // Minimum number of list items for which to show the tabs tray as expanded.
        const val EXPAND_AT_LIST_SIZE = 4

        // Minimum number of grid items for which to show the tabs tray as expanded.
        private const val EXPAND_AT_GRID_SIZE = 3

        // Elevation for undo toasts
        @VisibleForTesting
        internal const val ELEVATION = 80f
    }
}
