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
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.component_tabstray2.*
import kotlinx.android.synthetic.main.component_tabstray2.view.*
import kotlinx.android.synthetic.main.component_tabstray2.view.tab_tray_overflow
import kotlinx.android.synthetic.main.component_tabstray2.view.tab_wrapper
import kotlinx.android.synthetic.main.component_tabstray_fab.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.*
import kotlinx.android.synthetic.main.tabs_tray_tab_counter2.*
import kotlinx.android.synthetic.main.tabstray_multiselect_items.*
import kotlinx.android.synthetic.main.tabstray_multiselect_items.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.plus
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.concept.tabstray.Tab
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
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsInteractor
import kotlin.math.max

@Suppress("TooManyFunctions", "LargeClass")
class TabsTrayFragment : AppCompatDialogFragment(), TabsTrayInteractor {

    private var fabView: View? = null
    private lateinit var tabsTrayStore: TabsTrayStore
    private lateinit var browserTrayInteractor: BrowserTrayInteractor
    private lateinit var tabsTrayController: DefaultTabsTrayController
    private lateinit var behavior: BottomSheetBehavior<ConstraintLayout>

    private val tabLayoutMediator = ViewBoundFeatureWrapper<TabLayoutMediator>()
    private val tabCounterBinding = ViewBoundFeatureWrapper<TabCounterBinding>()
    private val floatingActionButtonBinding = ViewBoundFeatureWrapper<FloatingActionButtonBinding>()
    private val selectionBannerBinding = ViewBoundFeatureWrapper<SelectionBannerBinding>()
    private val selectionHandleBinding = ViewBoundFeatureWrapper<SelectionHandleBinding>()
    private val tabsTrayCtaBinding = ViewBoundFeatureWrapper<TabsTrayInfoBannerBinding>()
    private val closeOnLastTabBinding = ViewBoundFeatureWrapper<CloseOnLastTabBinding>()

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
                tabsTrayStore = tabsTrayStore,
                browserStore = requireComponents.core.store,
                navController = findNavController(),
                metrics = requireComponents.analytics.metrics,
                dismissTabTray = ::dismissTabsTray,
                dismissTabTrayAndNavigateHome = ::dismissTabsTrayAndNavigateHome,
                bookmarksUseCase = requireComponents.useCases.bookmarksUseCases,
                collectionStorage = requireComponents.core.tabCollectionStorage
            )

        tabsTrayController = DefaultTabsTrayController(
            store = tabsTrayStore,
            browsingModeManager = activity.browsingModeManager,
            navController = findNavController(),
            navigationInteractor = navigationInteractor,
            profiler = requireComponents.core.engine.profiler,
            accountManager = requireComponents.backgroundServices.accountManager,
            metrics = requireComponents.analytics.metrics,
            ioScope = lifecycleScope + Dispatchers.IO
        )

        browserTrayInteractor = DefaultBrowserTrayInteractor(
            tabsTrayStore,
            this@TabsTrayFragment,
            tabsTrayController,
            requireComponents.useCases.tabsUseCases.selectTab,
            requireComponents.settings,
            requireComponents.analytics.metrics
        )

        val syncedTabsTrayInteractor = SyncedTabsInteractor(
            requireComponents.analytics.metrics,
            requireActivity() as HomeActivity,
            this,
            controller = tabsTrayController
        )

        setupMenu(view, navigationInteractor)
        setupPager(
            view.context,
            tabsTrayStore,
            this,
            browserTrayInteractor,
            syncedTabsTrayInteractor
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
                interactor = this,
                browserStore = requireComponents.core.store,
                trayStore = tabsTrayStore,
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
                actionButton = new_tab_button,
                browserTrayInteractor = browserTrayInteractor,
                syncedTabsInteractor = syncedTabsTrayInteractor
            ),
            owner = this,
            view = view
        )

        selectionBannerBinding.set(
            feature = SelectionBannerBinding(
                context = requireContext(),
                store = tabsTrayStore,
                navInteractor = navigationInteractor,
                tabsTrayInteractor = this,
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

        closeOnLastTabBinding.set(
            feature = CloseOnLastTabBinding(
                browserStore = requireComponents.core.store,
                tabsTrayStore = tabsTrayStore,
                navigationInteractor = navigationInteractor
            ),
            owner = this,
            view = view
        )
    }

    override fun setCurrentTrayPosition(position: Int, smoothScroll: Boolean) {
        tabsTray.setCurrentItem(position, smoothScroll)
    }

    override fun navigateToBrowser() {
        dismissTabsTray()

        val navController = findNavController()

        if (navController.currentDestination?.id == R.id.browserFragment) {
            return
        }

        if (!navController.popBackStack(R.id.browserFragment, false)) {
            navController.navigateBlockingForAsyncNavGraph(R.id.browserFragment)
        }
    }

    override fun onDeleteTab(tabId: String) {
        // TODO re-implement these methods
        // showUndoSnackbarForTab(sessionId)
        // removeIfNotLastTab(sessionId)

        // Temporary
        requireComponents.useCases.tabsUseCases.removeTab(tabId)
    }

    override fun onDeleteTabs(tabs: Collection<Tab>) {
        tabs.forEach {
            onDeleteTab(it.id)
        }
    }

    private fun setupPager(
        context: Context,
        store: TabsTrayStore,
        trayInteractor: TabsTrayInteractor,
        browserInteractor: BrowserTrayInteractor,
        syncedTabsTrayInteractor: SyncedTabsInteractor
    ) {
        tabsTray.apply {
            adapter = TrayPagerAdapter(
                context,
                store,
                browserInteractor,
                syncedTabsTrayInteractor,
                trayInteractor,
                requireComponents.core.store
            )
            isUserInputEnabled = false
        }
    }

    private fun setupMenu(view: View, navigationInteractor: NavigationInteractor) {
        view.tab_tray_overflow.setOnClickListener { anchor ->

            requireComponents.analytics.metrics.track(Event.TabsTrayMenuOpened)

            val menu = MenuIntegration(
                context = requireContext(),
                browserStore = requireComponents.core.store,
                tabsTrayStore = TabsTrayStore(),
                tabLayout = tab_layout,
                navigationInteractor = navigationInteractor
            ).build()

            menu.showWithTheme(anchor)
        }
    }

    private fun setupBackgroundDismissalListener(block: (View) -> Unit) {
        tabLayout.setOnClickListener(block)
        handle.setOnClickListener(block)
    }

    private val homeViewModel: HomeScreenViewModel by activityViewModels()

    private fun dismissTabsTrayAndNavigateHome(sessionId: String) {
        homeViewModel.sessionToDelete = sessionId
        val directions = NavGraphDirections.actionGlobalHome()
        findNavController().navigateBlockingForAsyncNavGraph(directions)
        dismissTabsTray()
    }

    private fun dismissTabsTray() {
        dismissAllowingStateLoss()
        requireComponents.analytics.metrics.track(Event.TabsTrayClosed)
    }

    companion object {
        // Minimum number of list items for which to show the tabs tray as expanded.
        const val EXPAND_AT_LIST_SIZE = 4

        // Minimum number of grid items for which to show the tabs tray as expanded.
        private const val EXPAND_AT_GRID_SIZE = 3
    }
}
