/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.cardview.widget.CardView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.ContextCompat
import androidx.fragment.app.activityViewModels
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.android.synthetic.main.component_tabstray2.*
import kotlinx.android.synthetic.main.component_tabstray2.view.*
import kotlinx.android.synthetic.main.tabs_tray_tab_counter2.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.DefaultBrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.RemoveTabUseCaseWrapper
import org.mozilla.fenix.tabstray.browser.SelectTabUseCaseWrapper
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsInteractor

class TabsTrayFragment : AppCompatDialogFragment(), TabsTrayInteractor {

    private lateinit var navigationInteractor: NavigationInteractor

    private val tabLayout: TabLayout? get() =
        view?.tab_layout

    private val isPrivateModeSelected: Boolean get() =
        tabLayout?.selectedTabPosition == TrayPagerAdapter.POSITION_PRIVATE_TABS
    private lateinit var tabsTrayStore: TabsTrayStore
    private lateinit var browserTrayInteractor: BrowserTrayInteractor
    private lateinit var behavior: BottomSheetBehavior<ConstraintLayout>

    private val tabLayoutMediator = ViewBoundFeatureWrapper<TabLayoutMediator>()
    private val tabCounterBinding = ViewBoundFeatureWrapper<TabCounterBinding>()

    private val selectTabUseCase by lazy {
        SelectTabUseCaseWrapper(
            requireComponents.analytics.metrics,
            requireComponents.useCases.tabsUseCases.selectTab
        ) {
            navigateToBrowser()
        }
    }

    private val removeUseCases by lazy {
        RemoveTabUseCaseWrapper(
            requireComponents.analytics.metrics
        ) {
            tabRemoved(it)
        }
    }

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
        val activity = activity as HomeActivity

        behavior = BottomSheetBehavior.from(view.tab_wrapper)

        navigationInteractor =
            DefaultNavigationInteractor(
                browserStore = activity.components.core.store,
                navController = findNavController(),
                metrics = activity.components.analytics.metrics,
                dismissTabTray = ::dismissAllowingStateLoss,
                dismissTabTrayAndNavigateHome = ::dismissTabTrayAndNavigateHome
            )

        tabsTrayStore = StoreProvider.get(this) { TabsTrayStore() }

        return containerView
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupMenu(view)

        browserTrayInteractor = DefaultBrowserTrayInteractor(
            tabsTrayStore,
            selectTabUseCase,
            removeUseCases,
            requireComponents.settings,
            this
        )

        val syncedTabsTrayInteractor = SyncedTabsInteractor(
            requireComponents.analytics.metrics,
            requireActivity() as HomeActivity,
            this
        )

        setupPager(
            view.context,
            tabsTrayStore,
            this,
            browserTrayInteractor,
            syncedTabsTrayInteractor
        )

        tabLayoutMediator.set(
            feature = TabLayoutMediator(
                tabLayout = tab_layout,
                interactor = this,
                store = requireComponents.core.store
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
    }

    override fun setCurrentTrayPosition(position: Int, smoothScroll: Boolean) {
        tabsTray.setCurrentItem(position, smoothScroll)
    }

    override fun navigateToBrowser() {
        dismissAllowingStateLoss()

        val navController = findNavController()

        if (navController.currentDestination?.id == R.id.browserFragment) {
            return
        }

        if (!navController.popBackStack(R.id.browserFragment, false)) {
            navController.navigate(R.id.browserFragment)
        }
    }

    override fun tabRemoved(tabId: String) {
        // TODO re-implement these methods
        // showUndoSnackbarForTab(sessionId)
        // removeIfNotLastTab(sessionId)

        // Temporary
        requireComponents.useCases.tabsUseCases.removeTab(tabId)
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
                trayInteractor
            )
            isUserInputEnabled = false
        }
    }

    private fun setupMenu(view: View) {
        view.tab_tray_overflow.setOnClickListener { anchor ->
            val tabTrayItemMenu =
                TabsTrayMenu(
                    context = view.context,
                    browserStore = requireComponents.core.store,
                    tabLayout = tab_layout
                ) {
                    when (it) {
                        is TabsTrayMenu.Item.ShareAllTabs ->
                            navigationInteractor.onShareTabsOfTypeClicked(isPrivateModeSelected)
                        is TabsTrayMenu.Item.OpenTabSettings ->
                            navigationInteractor.onTabSettingsClicked()
                        is TabsTrayMenu.Item.CloseAllTabs ->
                            navigationInteractor.onCloseAllTabsClicked(isPrivateModeSelected)
                        is TabsTrayMenu.Item.OpenRecentlyClosed ->
                            navigationInteractor.onOpenRecentlyClosedClicked()
                        is TabsTrayMenu.Item.SelectTabs ->
                        { /* TODO implement when mulitiselect call is available */ }
                    }
                }

            requireComponents.analytics.metrics.track(Event.TabsTrayMenuOpened)
            val menu = tabTrayItemMenu.menuBuilder.build(view.context)
            menu.show(anchor).also { popupMenu ->
                (popupMenu.contentView as? CardView)?.setCardBackgroundColor(
                    ContextCompat.getColor(
                        view.context,
                        R.color.foundation_normal_theme
                    )
                )
            }
        }
    }
    private val homeViewModel: HomeScreenViewModel by activityViewModels()

    private fun dismissTabTrayAndNavigateHome(sessionId: String) {
        homeViewModel.sessionToDelete = sessionId
        val directions = NavGraphDirections.actionGlobalHome()
        findNavController().navigate(directions)
        dismissAllowingStateLoss()
    }
}
