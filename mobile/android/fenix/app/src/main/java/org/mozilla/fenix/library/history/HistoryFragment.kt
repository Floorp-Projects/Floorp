/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.SpannableString
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.RadioGroup
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.core.view.MenuProvider
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDirections
import androidx.navigation.fragment.findNavController
import androidx.paging.Pager
import androidx.paging.PagingConfig
import androidx.paging.PagingData
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.kotlin.toShortUrl
import mozilla.components.ui.widgets.withCenterAlignedButtons
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavHostActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.history.DefaultPagedHistoryProvider
import org.mozilla.fenix.databinding.FragmentHistoryBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.library.history.state.HistoryNavigationMiddleware
import org.mozilla.fenix.library.history.state.HistoryStorageMiddleware
import org.mozilla.fenix.library.history.state.HistorySyncMiddleware
import org.mozilla.fenix.library.history.state.HistoryTelemetryMiddleware
import org.mozilla.fenix.library.history.state.bindings.MenuBinding
import org.mozilla.fenix.library.history.state.bindings.PendingDeletionBinding
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.GleanMetrics.History as GleanHistory

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HistoryFragment : LibraryPageFragment<History>(), UserInteractionHandler, MenuProvider {
    private lateinit var historyStore: HistoryFragmentStore
    private lateinit var historyProvider: DefaultPagedHistoryProvider

    private var deleteHistory: MenuItem? = null

    private var history: Flow<PagingData<History>> = Pager(
        PagingConfig(PAGE_SIZE),
        null,
    ) {
        HistoryDataSource(
            historyProvider = historyProvider,
        )
    }.flow

    private var _historyView: HistoryView? = null
    private val historyView: HistoryView
        get() = _historyView!!
    private var _binding: FragmentHistoryBinding? = null
    private val binding get() = _binding!!

    private val pendingDeletionBinding by lazy {
        PendingDeletionBinding(requireContext().components.appStore, historyView)
    }

    private val menuBinding by lazy {
        MenuBinding(
            store = historyStore,
            invalidateOptionsMenu = { activity?.invalidateOptionsMenu() },
        )
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentHistoryBinding.inflate(inflater, container, false)
        val view = binding.root
        historyStore = StoreProvider.get(this) {
            HistoryFragmentStore(
                initialState = HistoryFragmentState.initial,
                middleware = listOf(
                    HistoryNavigationMiddleware(
                        navController = findNavController(),
                        openToBrowser = ::openItem,
                        onBackPressed = requireActivity().onBackPressedDispatcher::onBackPressed,
                    ),
                    HistoryTelemetryMiddleware(
                        isInPrivateMode = requireComponents.appStore.state.mode == BrowsingMode.Private,
                    ),
                    HistorySyncMiddleware(
                        accountManager = requireContext().components.backgroundServices.accountManager,
                        refreshView = { historyView.historyAdapter.refresh() },
                        scope = lifecycleScope,
                    ),
                    HistoryStorageMiddleware(
                        appStore = requireContext().components.appStore,
                        browserStore = requireContext().components.core.store,
                        historyProvider = historyProvider,
                        historyStorage = requireContext().components.core.historyStorage,
                        undoDeleteSnackbar = ::showDeleteSnackbar,
                        onTimeFrameDeleted = ::onTimeFrameDeleted,
                    ),
                ),
            )
        }
        _historyView = HistoryView(
            binding.historyLayout,
            onZeroItemsLoaded = {
                historyStore.dispatch(
                    HistoryFragmentAction.ChangeEmptyState(isEmpty = true),
                )
            },
            store = historyStore,
            onEmptyStateChanged = {
                historyStore.dispatch(
                    HistoryFragmentAction.ChangeEmptyState(it),
                )
            },
        )

        return view
    }

    /**
     * All the current selected items. Individual history entries and entries from a group.
     * When a history group is selected, this will instead contain all the history entries in that group.
     */
    override val selectedItems
        get() = historyStore.state.mode.selectedItems.fold(emptyList<History>()) { accumulator, item ->
            when (item) {
                is History.Group -> accumulator + item.items
                else -> accumulator + item
            }
        }.toSet()

    private fun invalidateOptionsMenu() {
        activity?.invalidateOptionsMenu()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        historyProvider = DefaultPagedHistoryProvider(requireComponents.core.historyStorage)

        GleanHistory.opened.record(NoExtras())
    }

    private fun showDeleteSnackbar(
        items: Set<History>,
        undo: suspend (Set<History>) -> Unit,
        delete: suspend (Set<History>) -> Unit,
    ) {
        CoroutineScope(IO).allowUndo(
            view = requireActivity().getRootView()!!,
            message = getMultiSelectSnackBarMessage(items),
            undoActionTitle = getString(R.string.snackbar_deleted_undo),
            onCancel = { undo.invoke(items) },
            operation = { delete(items) },
        )
    }

    private fun onTimeFrameDeleted() {
        runIfFragmentIsAttached {
            historyView.historyAdapter.refresh()
            showSnackBar(
                binding.root,
                getString(R.string.preferences_delete_browsing_data_snackbar),
            )
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        consumeFrom(historyStore) {
            historyView.update(it)
            updateDeleteMenuItemView(!it.isEmpty)
        }

        requireContext().components.appStore.flowScoped(viewLifecycleOwner) { flow ->
            flow.mapNotNull { state -> state.pendingDeletionHistoryItems }.collect { items ->
                historyStore.dispatch(
                    HistoryFragmentAction.UpdatePendingDeletionItems(pendingDeletionItems = items),
                )
            }
        }

        viewLifecycleOwner.lifecycleScope.launch {
            history.collect {
                historyView.historyAdapter.submitData(it)
            }
        }

        startStateBindings()
    }

    private fun startStateBindings() {
        pendingDeletionBinding.start()
        menuBinding.start()
    }

    private fun stopStateBindings() {
        pendingDeletionBinding.stop()
        menuBinding.stop()
    }

    private fun updateDeleteMenuItemView(isEnabled: Boolean) {
        val closedTabs = requireContext().components.core.store.state.closedTabs.size
        if (!isEnabled && closedTabs == 0) {
            deleteHistory?.isEnabled = false
            deleteHistory?.icon?.setTint(
                ContextCompat.getColor(requireContext(), R.color.fx_mobile_icon_color_disabled),
            )
        }
    }

    override fun onResume() {
        super.onResume()

        (activity as NavHostActivity).getSupportActionBarAndInflateIfNecessary().show()
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        if (historyStore.state.mode is HistoryFragmentState.Mode.Editing) {
            inflater.inflate(R.menu.history_select_multi, menu)
            menu.findItem(R.id.share_history_multi_select)?.isVisible = true
            menu.findItem(R.id.delete_history_multi_select)?.title =
                SpannableString(getString(R.string.bookmark_menu_delete_button)).apply {
                    setTextColor(requireContext(), R.attr.textWarning)
                }
        } else {
            inflater.inflate(R.menu.history_menu, menu)
            deleteHistory = menu.findItem(R.id.history_delete)
            updateDeleteMenuItemView(!historyStore.state.isEmpty)
        }
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.share_history_multi_select -> {
            val selectedHistory = historyStore.state.mode.selectedItems
            val shareTabs = mutableListOf<ShareData>()

            for (history in selectedHistory) {
                when (history) {
                    is History.Regular -> {
                        shareTabs.add(ShareData(url = history.url, title = history.title))
                    }
                    is History.Group -> {
                        shareTabs.addAll(
                            history.items.map { metadata ->
                                ShareData(url = metadata.url, title = metadata.title)
                            },
                        )
                    }
                    else -> {
                        // no-op, There is no [History.Metadata] in the HistoryFragment.
                    }
                }
            }

            share(shareTabs)
            historyStore.dispatch(HistoryFragmentAction.ExitEditMode)
            true
        }
        R.id.delete_history_multi_select -> {
            with(historyStore) {
                dispatch(HistoryFragmentAction.DeleteItems(state.mode.selectedItems))
                dispatch(HistoryFragmentAction.ExitEditMode)
            }
            true
        }
        R.id.open_history_in_new_tabs_multi_select -> {
            openItemsInNewTab { selectedItem ->
                GleanHistory.openedItemsInNewTabs.record(NoExtras())
                (selectedItem as? History.Regular)?.url ?: (selectedItem as? History.Metadata)?.url
            }

            showTabTray()
            historyStore.dispatch(HistoryFragmentAction.ExitEditMode)
            true
        }
        R.id.open_history_in_private_tabs_multi_select -> {
            openItemsInNewTab(private = true) { selectedItem ->
                GleanHistory.openedItemsInNewTabs.record(NoExtras())
                (selectedItem as? History.Regular)?.url ?: (selectedItem as? History.Metadata)?.url
            }

            (activity as HomeActivity).supportActionBar?.hide()

            showTabTray(openInPrivate = true)
            historyStore.dispatch(HistoryFragmentAction.ExitEditMode)
            true
        }
        R.id.history_search -> {
            historyStore.dispatch(HistoryFragmentAction.SearchClicked)
            true
        }
        R.id.history_delete -> {
            DeleteConfirmationDialogFragment(
                store = historyStore,
            ).show(childFragmentManager, null)
            true
        }
        // other options are not handled by this menu provider
        else -> false
    }

    private fun showTabTray(openInPrivate: Boolean = false) {
        findNavController().nav(
            R.id.historyFragment,
            HistoryFragmentDirections.actionGlobalTabsTrayFragment(
                page = if (openInPrivate) {
                    Page.PrivateTabs
                } else {
                    Page.NormalTabs
                },
            ),
        )
    }

    private fun getMultiSelectSnackBarMessage(historyItems: Set<History>): String {
        return if (historyItems.size > 1) {
            getString(R.string.history_delete_multiple_items_snackbar)
        } else {
            val historyItem = historyItems.first()

            String.format(
                requireContext().getString(R.string.history_delete_single_item_snackbar),
                if (historyItem is History.Regular) {
                    historyItem.url.toShortUrl(requireComponents.publicSuffixList)
                } else {
                    historyItem.title
                },
            )
        }
    }

    override fun onBackPressed(): Boolean {
        // The state needs to be updated accordingly if Edit mode is active
        historyStore.dispatch(HistoryFragmentAction.BackPressed)
        return true
    }

    override fun onDestroyView() {
        super.onDestroyView()
        stopStateBindings()
        _historyView = null
        _binding = null
    }

    private fun openItem(item: History.Regular) = runIfFragmentIsAttached {
        GleanHistory.openedItem.record(
            GleanHistory.OpenedItemExtra(
                isRemote = item.isRemote,
                timeGroup = item.historyTimeGroup.toString(),
                isPrivate = requireComponents.appStore.state.mode == BrowsingMode.Private,
            ),
        )

        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = item.url,
            newTab = true,
            from = BrowserDirection.FromHistory,
        )
    }

    private fun share(data: List<ShareData>) {
        GleanHistory.shared.record(NoExtras())
        val directions = HistoryFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray(),
        )
        navigateToHistoryFragment(directions)
    }

    private fun navigateToHistoryFragment(directions: NavDirections) {
        findNavController().nav(
            R.id.historyFragment,
            directions,
        )
    }

    @Suppress("UnusedPrivateMember")
    private suspend fun syncHistory() {
        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.syncNow(
            reason = SyncReason.User,
            debounce = true,
            customEngineSubset = listOf(SyncEngine.History),
        )
        historyView.historyAdapter.refresh()
    }

    internal class DeleteConfirmationDialogFragment(
        private val store: HistoryFragmentStore,
    ) : DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
            AlertDialog.Builder(requireContext()).apply {
                val layout = LayoutInflater.from(context)
                    .inflate(R.layout.delete_history_time_range_dialog, null)
                val radioGroup = layout.findViewById<RadioGroup>(R.id.radio_group)
                radioGroup.check(R.id.last_hour_button)
                setView(layout)

                setNegativeButton(R.string.delete_browsing_data_prompt_cancel) { dialog: DialogInterface, _ ->
                    GleanHistory.removePromptCancelled.record(NoExtras())
                    dialog.cancel()
                }
                setPositiveButton(R.string.delete_browsing_data_prompt_allow) { dialog: DialogInterface, _ ->
                    val selectedTimeFrame = when (radioGroup.checkedRadioButtonId) {
                        R.id.last_hour_button -> RemoveTimeFrame.LastHour
                        R.id.today_and_yesterday_button -> RemoveTimeFrame.TodayAndYesterday
                        R.id.everything_button -> null
                        else -> throw IllegalStateException("Unexpected radioButtonId")
                    }
                    store.dispatch(HistoryFragmentAction.DeleteTimeRange(selectedTimeFrame))
                    dialog.dismiss()
                }

                GleanHistory.removePromptOpened.record(NoExtras())
            }.create().withCenterAlignedButtons()
    }

    @Suppress("UnusedPrivateMember")
    companion object {
        private const val PAGE_SIZE = 25
    }
}
