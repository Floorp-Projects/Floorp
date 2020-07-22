/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.fragment.app.FragmentManager
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.component_tabstray_fab.view.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class TabTrayDialogFragment : AppCompatDialogFragment() {
    private val tabsFeature = ViewBoundFeatureWrapper<TabsFeature>()
    private var _tabTrayView: TabTrayView? = null
    private val tabTrayView: TabTrayView
        get() = _tabTrayView!!

    private val snackbarAnchor: View?
        get() = if (tabTrayView.fabView.new_tab_button.isVisible) tabTrayView.fabView.new_tab_button
        else null

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            showCollectionSnackbar(sessions.size, true)
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            showCollectionSnackbar(sessions.size)
        }
    }

    private val selectTabUseCase = object : TabsUseCases.SelectTabUseCase {
        override fun invoke(tabId: String) {
            requireContext().components.analytics.metrics.track(Event.OpenedExistingTab)
            requireComponents.useCases.tabsUseCases.selectTab(tabId)
            navigateToBrowser()
        }

        override fun invoke(session: Session) {
            requireContext().components.analytics.metrics.track(Event.OpenedExistingTab)
            requireComponents.useCases.tabsUseCases.selectTab(session)
            navigateToBrowser()
        }
    }

    private val removeTabUseCase = object : TabsUseCases.RemoveTabUseCase {
        override fun invoke(sessionId: String) {
            requireContext().components.analytics.metrics.track(Event.ClosedExistingTab)
            showUndoSnackbarForTab(sessionId)
            removeIfNotLastTab(sessionId)
        }

        override fun invoke(session: Session) {
            requireContext().components.analytics.metrics.track(Event.ClosedExistingTab)
            showUndoSnackbarForTab(session.id)
            removeIfNotLastTab(session.id)
        }
    }

    private fun removeIfNotLastTab(sessionId: String) {
        // We only want to *immediately* remove a tab if there are more than one in the tab tray
        // If there is only one, the HomeFragment handles deleting the tab (to better support snackbars)
        val sessionManager = view?.context?.components?.core?.sessionManager
        val sessionToRemove = sessionManager?.findSessionById(sessionId)

        if (sessionManager?.sessions?.filter { sessionToRemove?.private == it.private }?.size != 1) {
            requireComponents.useCases.tabsUseCases.removeTab(sessionId)
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
            tabTrayView.dismissMenu()
            tabTrayView.expand()
        }
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate

        _tabTrayView = TabTrayView(
            view.tabLayout,
            interactor = TabTrayFragmentInteractor(
                DefaultTabTrayController(
                    activity = (activity as HomeActivity),
                    navController = findNavController(),
                    dismissTabTray = ::dismissAllowingStateLoss,
                    dismissTabTrayAndNavigateHome = ::dismissTabTrayAndNavigateHome,
                    registerCollectionStorageObserver = ::registerCollectionStorageObserver
                )
            ),
            isPrivate = isPrivate,
            startingInLandscape = requireContext().resources.configuration.orientation ==
                    Configuration.ORIENTATION_LANDSCAPE,
            lifecycleScope = viewLifecycleOwner.lifecycleScope
        ) { private ->
            val filter: (TabSessionState) -> Boolean = { state -> private == state.content.private }

            tabsFeature.get()?.filterTabs(filter)

            setSecureFlagsIfNeeded(private)
        }

        tabsFeature.set(
            TabsFeature(
                tabTrayView.view.tabsTray,
                view.context.components.core.store,
                selectTabUseCase,
                removeTabUseCase,
                { it.content.private == isPrivate },
                { }
            ),
            owner = viewLifecycleOwner,
            view = view
        )

        tabLayout.setOnClickListener {
            requireContext().components.analytics.metrics.track(Event.TabsTrayClosed)
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
        }
    }

    private fun setSecureFlagsIfNeeded(private: Boolean) {
        if (private && context?.settings()?.allowScreenshotsInPrivateMode == false) {
            dialog?.window?.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        } else if (!(activity as HomeActivity).browsingModeManager.mode.isPrivate) {
            dialog?.window?.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
        }
    }

    private fun showUndoSnackbarForTab(sessionId: String) {
        val sessionManager = view?.context?.components?.core?.sessionManager

        val snapshot = sessionManager
            ?.findSessionById(sessionId)?.let {
                sessionManager.createSessionSnapshot(it)
            } ?: return

        // Check if this is the last tab of this session type
        val isLastOpenTab = sessionManager.sessions.filter { snapshot.session.private == it.private }.size == 1

        if (isLastOpenTab) {
            dismissTabTrayAndNavigateHome(sessionId)
            return
        }

        val state = snapshot.engineSession?.saveState()
        val isSelected = sessionId == requireComponents.core.store.state.selectedTabId ?: false

        val snackbarMessage = if (snapshot.session.private) {
            getString(R.string.snackbar_private_tab_closed)
        } else {
            getString(R.string.snackbar_tab_closed)
        }

        lifecycleScope.allowUndo(
            requireView().tabLayout,
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo),
            {
                sessionManager.add(snapshot.session, isSelected, engineSessionState = state)
                _tabTrayView?.scrollToTab(snapshot.session.id)
            },
            operation = { },
            elevation = ELEVATION,
            anchorView = snackbarAnchor
        )
    }

    private fun dismissTabTrayAndNavigateHome(sessionId: String) {
        val directions = BrowserFragmentDirections.actionGlobalHome(sessionToDelete = sessionId)
        findNavController().navigate(directions)
        dismissAllowingStateLoss()
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

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    private fun showCollectionSnackbar(tabSize: Int, isNewCollection: Boolean = false) {
        view.let {
            val messageStringRes = when {
                isNewCollection -> {
                    R.string.create_collection_tabs_saved_new_collection
                }
                tabSize > 1 -> {
                    R.string.create_collection_tabs_saved
                }
                else -> {
                    R.string.create_collection_tab_saved
                }
            }
            val snackbar = FenixSnackbar
                .make(
                    duration = FenixSnackbar.LENGTH_LONG,
                    isDisplayedWithBrowserToolbar = true,
                    view = (view as View)
                )
                .setAnchorView(snackbarAnchor)
                .setText(requireContext().getString(messageStringRes))
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
            if (fragmentManager.isDestroyed) {
                return
            }

            // We want to make sure we don't accidentally show the dialog twice if
            // a user somehow manages to trigger `show()` twice before we present the dialog.
            if (fragmentManager.findFragmentByTag(FRAGMENT_TAG) == null) {
                TabTrayDialogFragment().showNow(fragmentManager, FRAGMENT_TAG)
            }
        }
    }
}
