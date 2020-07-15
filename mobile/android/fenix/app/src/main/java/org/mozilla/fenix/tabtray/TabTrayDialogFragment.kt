/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.app.Dialog
import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.component_tabstray_fab.view.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.*
import kotlinx.android.synthetic.main.fragment_tab_tray_dialog.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getDefaultCollectionNumber
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.normalSessionSize
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentState.Mode
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class TabTrayDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {
    private val args by navArgs<TabTrayDialogFragmentArgs>()

    private val tabsFeature = ViewBoundFeatureWrapper<TabsFeature>()
    private var _tabTrayView: TabTrayView? = null
    private var currentOrientation: Int? = null
    private val tabTrayView: TabTrayView
        get() = _tabTrayView!!
    private lateinit var tabTrayDialogStore: TabTrayDialogFragmentStore

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

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return object : Dialog(requireContext(), this.theme) {
            override fun onBackPressed() {
                this@TabTrayDialogFragment.onBackPressed()
            }
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
    ): View? {
        tabTrayDialogStore = StoreProvider.get(this) {
            TabTrayDialogFragmentStore(
                TabTrayDialogFragmentState(
                    requireComponents.core.store.state,
                    if (args.enterMultiselect) Mode.MultiSelect(setOf()) else Mode.Normal
                )
            )
        }

        return inflater.inflate(R.layout.fragment_tab_tray_dialog, container, false)
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        val isLandscape = newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE
        tabTrayView.setTopOffset(isLandscape)

        if (newConfig.orientation != currentOrientation) {
            tabTrayView.dismissMenu()
            tabTrayView.expand()
            currentOrientation = newConfig.orientation
        }
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val activity = activity as HomeActivity
        val isPrivate = activity.browsingModeManager.mode.isPrivate

        val thumbnailLoader = ThumbnailLoader(requireContext().components.core.thumbnailStorage)
        val adapter = FenixTabsAdapter(requireContext(), thumbnailLoader)
        currentOrientation = resources.configuration.orientation

        _tabTrayView = TabTrayView(
            view.tabLayout,
            adapter,
            interactor = TabTrayFragmentInteractor(
                DefaultTabTrayController(
                    profiler = activity.components.core.engine.profiler,
                    sessionManager = activity.components.core.sessionManager,
                    browsingModeManager = activity.browsingModeManager,
                    tabCollectionStorage = activity.components.core.tabCollectionStorage,
                    navController = findNavController(),
                    dismissTabTray = ::dismissAllowingStateLoss,
                    dismissTabTrayAndNavigateHome = ::dismissTabTrayAndNavigateHome,
                    registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                    tabTrayDialogFragmentStore = tabTrayDialogStore,
                    selectTabUseCase = selectTabUseCase,
                    showChooseCollectionDialog = ::showChooseCollectionDialog,
                    showAddNewCollectionDialog = ::showAddNewCollectionDialog
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
                adapter,
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
            tabTrayDialogStore.dispatch(TabTrayDialogFragmentAction.BrowserStateChanged(it))
        }

        consumeFrom(tabTrayDialogStore) {
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
        val isLastOpenTab =
            sessionManager.sessions.filter { snapshot.session.private == it.private }.size == 1

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

    override fun onBackPressed(): Boolean {
        if (!tabTrayView.onBackPressed()) {
            dismiss()
        }
        return true
    }

    private fun showChooseCollectionDialog(sessionList: List<Session>) {
        context?.let {
            val tabCollectionStorage = it.components.core.tabCollectionStorage
            val collections =
                tabCollectionStorage.cachedTabCollections.map { it.title }.toTypedArray()
            val customLayout =
                LayoutInflater.from(it).inflate(R.layout.add_new_collection_dialog, null)
            val list = customLayout.findViewById<RecyclerView>(R.id.recycler_view)
            list.layoutManager = LinearLayoutManager(it)

            val builder = AlertDialog.Builder(it).setTitle(R.string.tab_tray_select_collection)
                .setView(customLayout)
                .setPositiveButton(android.R.string.ok) { dialog, _ ->
                    val selectedCollection =
                        (list.adapter as CollectionsAdapter).getSelectedCollection()
                    val collection = tabCollectionStorage.cachedTabCollections[selectedCollection]
                    viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
                        tabCollectionStorage.addTabsToCollection(collection, sessionList)
                        it.metrics.track(
                            Event.CollectionTabsAdded(
                                it.components.core.sessionManager.normalSessionSize(),
                                sessionList.size
                            )
                        )
                        launch(Main) {
                            tabTrayDialogStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
                            dialog.dismiss()
                        }
                    }
                }.setNegativeButton(android.R.string.cancel) { dialog, _ ->
                    tabTrayDialogStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
                    dialog.cancel()
                }

            val dialog = builder.create()
            val adapter =
                CollectionsAdapter(arrayOf(it.getString(R.string.tab_tray_add_new_collection)) + collections) {
                    dialog.dismiss()
                    showAddNewCollectionDialog(sessionList)
                }
            list.adapter = adapter
            dialog.show()
        }
    }

    private fun showAddNewCollectionDialog(sessionList: List<Session>) {
        context?.let {
            val tabCollectionStorage = it.components.core.tabCollectionStorage
            val customLayout =
                LayoutInflater.from(it).inflate(R.layout.name_collection_dialog, null)
            val collectionNameEditText: EditText =
                customLayout.findViewById(R.id.collection_name)
            collectionNameEditText.setText(
                it.getString(
                    R.string.create_collection_default_name,
                    tabCollectionStorage.cachedTabCollections.getDefaultCollectionNumber()
                )
            )

            AlertDialog.Builder(it).setTitle(R.string.tab_tray_add_new_collection)
                .setView(customLayout).setPositiveButton(android.R.string.ok) { dialog, _ ->
                    viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
                        tabCollectionStorage.createCollection(
                            collectionNameEditText.text.toString(),
                            sessionList
                        )
                        it.metrics.track(
                            Event.CollectionSaved(
                                it.components.core.sessionManager.normalSessionSize(),
                                sessionList.size
                            )
                        )
                        launch(Main) {
                            tabTrayDialogStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
                            dialog.dismiss()
                        }
                    }
                }.setNegativeButton(android.R.string.cancel) { dialog, _ ->
                    tabTrayDialogStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
                    dialog.cancel()
                }.create().show().also {
                    collectionNameEditText.setSelection(0, collectionNameEditText.text.length)
                    collectionNameEditText.showKeyboard()
                }
        }
    }

    companion object {
        private const val ELEVATION = 80f
    }
}
