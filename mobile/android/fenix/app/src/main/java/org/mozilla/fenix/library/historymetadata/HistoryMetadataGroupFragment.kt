/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata

import android.os.Bundle
import android.text.SpannableString
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentHistoryMetadataGroupBinding
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.controller.DefaultHistoryMetadataGroupController
import org.mozilla.fenix.library.historymetadata.interactor.DefaultHistoryMetadataGroupInteractor
import org.mozilla.fenix.library.historymetadata.interactor.HistoryMetadataGroupInteractor
import org.mozilla.fenix.library.historymetadata.view.HistoryMetadataGroupView

/**
 * Displays a list of history metadata items for a history metadata search group.
 */
class HistoryMetadataGroupFragment :
    LibraryPageFragment<History.Metadata>(), UserInteractionHandler {

    private lateinit var historyMetadataGroupStore: HistoryMetadataGroupFragmentStore
    private lateinit var interactor: HistoryMetadataGroupInteractor

    private var _historyMetadataGroupView: HistoryMetadataGroupView? = null
    private val historyMetadataGroupView: HistoryMetadataGroupView
        get() = _historyMetadataGroupView!!
    private var _binding: FragmentHistoryMetadataGroupBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<HistoryMetadataGroupFragmentArgs>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentHistoryMetadataGroupBinding.inflate(inflater, container, false)

        historyMetadataGroupStore = StoreProvider.get(this) {
            HistoryMetadataGroupFragmentStore(
                HistoryMetadataGroupFragmentState(
                    items = args.historyMetadataItems.filterIsInstance<History.Metadata>()
                )
            )
        }

        interactor = DefaultHistoryMetadataGroupInteractor(
            controller = DefaultHistoryMetadataGroupController(
                activity = activity as HomeActivity,
                store = historyMetadataGroupStore,
                metrics = requireComponents.analytics.metrics,
                navController = findNavController(),
                scope = lifecycleScope,
                searchTerm = args.title
            )
        )

        _historyMetadataGroupView = HistoryMetadataGroupView(
            container = binding.historyMetadataGroupLayout,
            interactor = interactor,
            title = args.title
        )

        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        consumeFrom(historyMetadataGroupStore) { state ->
            historyMetadataGroupView.update(state)
            activity?.invalidateOptionsMenu()
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(args.title)
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        if (selectedItems.isNotEmpty()) {
            inflater.inflate(R.menu.history_select_multi, menu)

            menu.findItem(R.id.delete_history_multi_select)?.let { deleteItem ->
                deleteItem.title = SpannableString(deleteItem.title).apply {
                    setTextColor(requireContext(), R.attr.destructive)
                }
            }
        } else {
            inflater.inflate(R.menu.history_menu, menu)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.share_history_multi_select -> {
                interactor.onShareMenuItem(selectedItems)
                true
            }
            R.id.delete_history_multi_select -> {
                interactor.onDelete(selectedItems)
                true
            }
            R.id.open_history_in_new_tabs_multi_select -> {
                openItemsInNewTab { selectedItem ->
                    selectedItem.url
                }

                showTabTray()
                true
            }
            R.id.open_history_in_private_tabs_multi_select -> {
                openItemsInNewTab(private = true) { selectedItem ->
                    selectedItem.url
                }

                (activity as HomeActivity).apply {
                    browsingModeManager.mode = BrowsingMode.Private
                    supportActionBar?.hide()
                }

                showTabTray()
                true
            }
            R.id.history_delete_all -> {
                interactor.onDeleteAllMenuItem()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _historyMetadataGroupView = null
        _binding = null
    }

    override val selectedItems: Set<History.Metadata>
        get() =
            historyMetadataGroupStore.state.items.filter { it.selected }.toSet()

    override fun onBackPressed(): Boolean = interactor.onBackPressed(selectedItems)

    private fun showTabTray() {
        findNavController().nav(
            R.id.historyMetadataGroupFragment,
            HistoryMetadataGroupFragmentDirections.actionGlobalTabsTrayFragment()
        )
    }
}
