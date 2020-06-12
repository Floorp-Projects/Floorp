/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.view.updatePadding
import androidx.fragment.app.FragmentManager
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.view.*
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.components.TabCollectionStorage

@SuppressWarnings("TooManyFunctions", "LargeClass")
class TabTrayDialogFragment : AppCompatDialogFragment(), TabTrayInteractor {
    private val tabsFeature = ViewBoundFeatureWrapper<TabsFeature>()
    private var _tabTrayView: TabTrayView? = null
    private val tabTrayView: TabTrayView
        get() = _tabTrayView!!

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            showCollectionSnackbar()
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            showCollectionSnackbar()
        }
    }

    private val selectTabUseCase = object : TabsUseCases.SelectTabUseCase {
        override fun invoke(tabId: String) {
            requireComponents.useCases.tabsUseCases.selectTab(tabId)
            navigateToBrowser()
        }

        override fun invoke(session: Session) {
            requireComponents.useCases.tabsUseCases.selectTab(session)
            navigateToBrowser()
        }
    }

    private val removeTabUseCase = object : TabsUseCases.RemoveTabUseCase {
        override fun invoke(sessionId: String) {
            showUndoSnackbarForTab(sessionId)
            requireComponents.useCases.tabsUseCases.removeTab(sessionId)
        }

        override fun invoke(session: Session) {
            showUndoSnackbarForTab(session.id)
            requireComponents.useCases.tabsUseCases.removeTab(session)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.TabTrayDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_tab_tray_dialog, container, false)

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        val isLandscape = newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE
        tabTrayView.setTopOffset(isLandscape)

        if (isLandscape) {
            tabTrayView.expand()
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate

        _tabTrayView = TabTrayView(
            view.tabLayout,
            this,
            isPrivate,
            requireContext().resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
        ) { tabsFeature.get()?.filterTabs(it) }

        tabsFeature.set(
            TabsFeature(
                tabTrayView.view.tabsTray,
                view.context.components.core.store,
                selectTabUseCase,
                removeTabUseCase,
                view.context.components.useCases.thumbnailUseCases,
                { it.content.private == isPrivate },
                { }
            ),
            owner = viewLifecycleOwner,
            view = view
        )

        tabLayout.setOnClickListener {
            dismissAllowingStateLoss()
        }

        view.tabLayout.setOnApplyWindowInsetsListener { v, insets ->
            v.updatePadding(
                left = insets.systemWindowInsetLeft,
                right = insets.systemWindowInsetRight,
                bottom = insets.systemWindowInsetBottom
            )

            tabTrayView.view.tab_wrapper.updatePadding(
                bottom = insets.systemWindowInsetBottom
            )

            insets
        }

        consumeFrom(requireComponents.core.store) {
            tabTrayView.updateState(it)
            navigateHomeIfNeeded(it)
        }
    }

    private fun showUndoSnackbarForTab(sessionId: String) {
        val sessionManager = view?.context?.components?.core?.sessionManager
        val snapshot = sessionManager
            ?.findSessionById(sessionId)?.let {
                sessionManager.createSessionSnapshot(it)
            } ?: return

        val state = snapshot.engineSession?.saveState()
        val isSelected = sessionId == requireComponents.core.store.state.selectedTabId ?: false

        val snackbarMessage = if (snapshot.session.private) {
            getString(R.string.snackbar_private_tab_closed)
        } else {
            getString(R.string.snackbar_tab_closed)
        }

        view?.tabLayout?.let {
            viewLifecycleOwner.lifecycleScope.allowUndo(
                it,
                snackbarMessage,
                getString(R.string.snackbar_deleted_undo),
                {
                    sessionManager.add(snapshot.session, isSelected, engineSessionState = state)
                },
                operation = { },
                elevation = ELEVATION
            )
        }
    }

    override fun onDestroyView() {
        _tabTrayView = null
        super.onDestroyView()
    }

    fun navigateToBrowser() {
        dismissAllowingStateLoss()
        if (findNavController().currentDestination?.id == R.id.browserFragment) return
        if (!findNavController().popBackStack(R.id.browserFragment, false)) {
            findNavController().navigate(R.id.browserFragment)
        }
    }

    override fun onNewTabTapped(private: Boolean) {
        (activity as HomeActivity).browsingModeManager.mode = BrowsingMode.fromBoolean(private)
        findNavController().navigate(TabTrayDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true))
        dismissAllowingStateLoss()
    }

    override fun onTabTrayDismissed() {
        dismissAllowingStateLoss()
    }

    override fun onSaveToCollectionClicked() {
        val tabs = getListOfSessions(false)
        val tabIds = tabs.map { it.id }.toList().toTypedArray()
        val tabCollectionStorage = (activity as HomeActivity).components.core.tabCollectionStorage
        val navController = findNavController()

        val step = when {
            // Show the SelectTabs fragment if there are multiple opened tabs to select which tabs
            // you want to save to a collection.
            tabs.size > 1 -> SaveCollectionStep.SelectTabs
            // If there is an existing tab collection, show the SelectCollection fragment to save
            // the selected tab to a collection of your choice.
            tabCollectionStorage.cachedTabCollections.isNotEmpty() -> SaveCollectionStep.SelectCollection
            // Show the NameCollection fragment to create a new collection for the selected tab.
            else -> SaveCollectionStep.NameCollection
        }

        if (navController.currentDestination?.id == R.id.collectionCreationFragment) return

        // Only register the observer right before moving to collection creation
        registerCollectionStorageObserver()

        val directions = TabTrayDialogFragmentDirections.actionGlobalCollectionCreationFragment(
            tabIds = tabIds,
            saveCollectionStep = step,
            selectedTabIds = tabIds
        )
        navController.navigate(directions)
    }

    override fun onShareTabsClicked(private: Boolean) {
        val tabs = getListOfSessions(private)
        val data = tabs.map {
            ShareData(url = it.url, title = it.title)
        }
        val directions = TabTrayDialogFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        findNavController().navigate(directions)
    }

    override fun onCloseAllTabsClicked(private: Boolean) {
        val sessionManager = requireContext().components.core.sessionManager
        val tabs = getListOfSessions(private)

        val selectedIndex = sessionManager
            .selectedSession?.let { sessionManager.sessions.indexOf(it) } ?: 0

        val snapshot = tabs
            .map(sessionManager::createSessionSnapshot)
            .map { it.copy(engineSession = null, engineSessionState = it.engineSession?.saveState()) }
            .let { SessionManager.Snapshot(it, selectedIndex) }

        tabs.forEach {
            sessionManager.remove(it)
        }

        val snackbarMessage = if (tabTrayView.isPrivateModeSelected) {
            getString(R.string.snackbar_private_tabs_closed)
        } else {
            getString(R.string.snackbar_tabs_closed)
        }

        viewLifecycleOwner.lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo),
            {
                sessionManager.restore(snapshot)
            },
            operation = { },
            elevation = ELEVATION
        )
    }

    private fun getListOfSessions(private: Boolean): List<Session> {
        return requireContext().components.core.sessionManager.sessionsOfType(private = private)
            .toList()
    }

    private fun navigateHomeIfNeeded(state: BrowserState) {
        val shouldPop = if (tabTrayView.isPrivateModeSelected) {
            state.privateTabs.isEmpty()
        } else {
            state.normalTabs.isEmpty()
        }

        if (shouldPop) {
            findNavController().popBackStack(R.id.homeFragment, false)
        }
    }

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    private fun showCollectionSnackbar() {
        view.let {
            val snackbar = FenixSnackbar
                .make(
                    duration = FenixSnackbar.LENGTH_LONG,
                    isDisplayedWithBrowserToolbar = true,
                    view = (view as View)
                )
                .setText(requireContext().getString(R.string.create_collection_tabs_saved))
                .setAction(requireContext().getString(R.string.create_collection_view)) {
                    dismissAllowingStateLoss()
                    findNavController().navigate(
                        TabTrayDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = false)
                    )
                }

            snackbar.view.elevation = ELEVATION
            snackbar.show()
        }
    }

    companion object {
        private const val ELEVATION = 80f
        private const val FRAGMENT_TAG = "tabTrayDialogFragment"

        fun show(fragmentManager: FragmentManager) {
            // If we've killed the fragmentManager. Let's not try to show the tabs tray.
            if (fragmentManager.isDestroyed) { return }

            // We want to make sure we don't accidentally show the dialog twice if
            // a user somehow manages to trigger `show()` twice before we present the dialog.
            if (fragmentManager.findFragmentByTag(FRAGMENT_TAG) == null) {
                TabTrayDialogFragment().showNow(fragmentManager, FRAGMENT_TAG)
            }
        }
    }
}
