/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.core.view.isVisible
import mozilla.components.concept.engine.prompt.ShareData
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import mozilla.components.feature.tabs.tabstray.TabsFeature
import kotlinx.android.synthetic.main.fragment_tab_tray.tabsTray
import kotlinx.android.synthetic.main.fragment_tab_tray.view.*
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.tabstray.BrowserTabsTray
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class TabTrayFragment : Fragment(R.layout.fragment_tab_tray), TabsTray.Observer, UserInteractionHandler {
    private var tabsFeature: TabsFeature? = null
    var tabTrayMenu: Menu? = null

    private val sessionManager: SessionManager
        get() = requireComponents.core.sessionManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        showToolbar(getString(R.string.tab_tray_title))
        onTabsChanged()

        (tabsTray as? BrowserTabsTray)?.also { tray ->
            TabsTouchHelper(tray.tabsAdapter).attachToRecyclerView(tray)
        }

        sessionManager.register(observer = object : SessionManager.Observer {
            override fun onSessionAdded(session: Session) {
                onTabsChanged()
            }

            override fun onSessionRemoved(session: Session) {
                onTabsChanged()
            }

            override fun onSessionsRestored() {
                onTabsChanged()
            }

            override fun onAllSessionsRemoved() {
                onTabsChanged()
            }
        }, owner = viewLifecycleOwner)

        tabsFeature = TabsFeature(
            tabsTray,
            requireComponents.core.store,
            requireComponents.useCases.tabsUseCases,
            { it.content.private == (activity as HomeActivity?)?.browsingModeManager?.mode?.isPrivate },
            ::closeTabsTray)

        view.tab_tray_open_new_tab.setOnClickListener {
            val directions = TabTrayFragmentDirections.actionGlobalSearch(null)
            findNavController().navigate(directions)
        }

        view.tab_tray_go_home.setOnClickListener {
            val directions = TabTrayFragmentDirections.actionGlobalHome()
            findNavController().navigate(directions)
        }

        view.private_browsing_button.setOnClickListener {
            val newMode = !(activity as HomeActivity).browsingModeManager.mode.isPrivate
            val invertedMode = BrowsingMode.fromBoolean(newMode)
            (activity as HomeActivity).browsingModeManager.mode = invertedMode
            tabsFeature?.filterTabs { tabSessionState ->
                tabSessionState.content.private == newMode
            }
        }

        view.save_to_collection_button.setOnClickListener {
            saveToCollection()
        }
    }

    override fun onResume() {
        super.onResume()

        onTabsChanged()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.tab_tray_menu, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu) {
        this.tabTrayMenu = menu
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.tab_tray_select_to_save_menu_item -> {
                saveToCollection()
                true
            }
            R.id.tab_tray_share_menu_item -> {
                share(getListOfSessions().toList())
                true
            }
            R.id.tab_tray_close_menu_item -> {
                closeAllTabs()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun closeAllTabs() {
        val tabs = getListOfSessions()

        val selectedIndex = sessionManager
            .selectedSession?.let { sessionManager.sessions.indexOf(it) } ?: 0

        val snapshot = tabs
            .map(sessionManager::createSessionSnapshot)
            .map { it.copy(engineSession = null, engineSessionState = it.engineSession?.saveState()) }
            .let { SessionManager.Snapshot(it, selectedIndex) }

        tabs.forEach {
            sessionManager.remove(it)
        }

        val isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate
        val snackbarMessage = if (isPrivate) {
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
            anchorView = view?.tab_tray_controls
        )
    }

    private fun saveToCollection() {
        val tabs = getListOfSessions()
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

        val directions = TabTrayFragmentDirections.actionTabTrayFragmentToCreateCollectionFragment(
            tabIds = tabIds,
            previousFragmentId = R.id.tabTrayFragment,
            saveCollectionStep = step,
            selectedTabIds = tabIds
        )
        navController.nav(R.id.tabTrayFragment, directions)
    }

    override fun onStart() {
        super.onStart()

        tabsTray.register(this)
        tabsFeature?.start()
    }

    override fun onStop() {
        super.onStop()

        tabsTray.unregister(this)
        tabsFeature?.stop()
    }

    override fun onBackPressed(): Boolean {
        if (getListOfSessions().isEmpty()) {
            findNavController().popBackStack(R.id.homeFragment, false)
            return true
        }

        return false
    }

    private fun closeTabsTray() {
        activity?.supportFragmentManager?.beginTransaction()?.apply {
            commit()
        }
    }

    override fun onTabClosed(tab: Tab) {
        val snapshot = sessionManager
            .findSessionById(tab.id)?.let {
            sessionManager.createSessionSnapshot(it)
        } ?: return

        val state = snapshot.engineSession?.saveState()
        val isSelected = tab.id == requireComponents.core.store.state.selectedTabId ?: false

        val snackbarMessage = if (snapshot.session.private) {
            getString(R.string.snackbar_private_tab_closed)
        } else {
            getString(R.string.snackbar_tab_closed)
        }

        viewLifecycleOwner.lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo),
            {
                sessionManager.add(snapshot.session, isSelected, engineSessionState = state)
            },
            operation = { },
            anchorView = view?.tab_tray_controls
        )
    }

    override fun onTabSelected(tab: Tab) {
        (activity as HomeActivity).openToBrowser(BrowserDirection.FromTabTray)
    }

    private fun getListOfSessions(): List<Session> {
        val isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate
        return sessionManager.sessionsOfType(private = isPrivate)
            .toList()
    }

    private fun share(tabs: List<Session>) {
        val data = tabs.map {
            ShareData(url = it.url, title = it.title)
        }
        val directions = TabTrayFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        nav(R.id.tabTrayFragment, directions)
    }

    private fun onTabsChanged() {
        val hasNoTabs = getListOfSessions().toList().isEmpty()
        val isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate

        view?.tab_tray_empty_view?.isVisible = hasNoTabs
        view?.tabsTray?.asView()?.isVisible = !hasNoTabs
        view?.save_to_collection_button?.isVisible = !hasNoTabs && !isPrivate

        setHasOptionsMenu(!hasNoTabs)

        if (hasNoTabs) {
            view?.announceForAccessibility(view?.context?.getString(R.string.no_open_tabs_description))
        }
    }
}
