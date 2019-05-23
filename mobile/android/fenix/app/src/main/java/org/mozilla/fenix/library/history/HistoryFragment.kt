/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.DialogInterface
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.os.Bundle
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.coroutineScope
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import java.net.MalformedURLException
import java.net.URL
import kotlin.coroutines.CoroutineContext

@SuppressWarnings("TooManyFunctions")
class HistoryFragment : Fragment(), CoroutineScope, BackHandler {

    private lateinit var job: Job
    private lateinit var historyComponent: HistoryComponent
    private val navigation by lazy { Navigation.findNavController(requireView()) }

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_history, container, false)
        historyComponent = HistoryComponent(
            view.history_layout,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                HistoryViewModel::class.java,
                HistoryViewModel.Companion::create
            )
        )
        return view
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()

        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.library_history)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    private fun openItem(item: HistoryItem) {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = item.url,
            newTab = false,
            from = BrowserDirection.FromHistory)
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        when (val mode = (historyComponent.uiView as HistoryUIView).mode) {
            HistoryState.Mode.Normal -> inflater.inflate(R.menu.library_menu, menu)
            is HistoryState.Mode.Editing -> {
                inflater.inflate(R.menu.history_select_multi, menu)
                menu.findItem(R.id.share_history_multi_select)?.run {
                    isVisible = mode.selectedItems.isNotEmpty()
                    icon.colorFilter = PorterDuffColorFilter(
                        ContextCompat.getColor(context!!, R.color.white_color),
                        PorterDuff.Mode.SRC_IN
                    )
                }
            }
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        launch(Dispatchers.IO) {
            reloadData()
        }
    }

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @SuppressWarnings("ComplexMethod")
    override fun onStart() {
        super.onStart()
        getAutoDisposeObservable<HistoryAction>()
            .subscribe {
                when (it) {
                    is HistoryAction.Open -> openItem(it.item)
                    is HistoryAction.EnterEditMode -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.EnterEditMode(it.item))
                    is HistoryAction.AddItemForRemoval -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.AddItemForRemoval(it.item))
                    is HistoryAction.RemoveItemForRemoval -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.RemoveItemForRemoval(it.item))
                    is HistoryAction.BackPressed -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.ExitEditMode)
                    is HistoryAction.Delete.All -> {
                        activity?.let {
                            AlertDialog.Builder(
                                ContextThemeWrapper(
                                    it,
                                    R.style.DialogStyle
                                )
                            ).apply {
                                setMessage(R.string.history_delete_all_dialog)
                                setNegativeButton(android.R.string.cancel) { dialog: DialogInterface, _ ->
                                    dialog.cancel()
                                }
                                setPositiveButton(android.R.string.ok) { dialog: DialogInterface, _ ->
                                    launch(Dispatchers.IO) {
                                        requireComponents.core.historyStorage.deleteEverything()
                                        reloadData()
                                    }
                                    dialog.dismiss()
                                }
                                create()
                            }.show()
                        }
                    }
                    is HistoryAction.Delete.One -> launch(Dispatchers.IO) {
                        requireComponents.core.historyStorage.deleteVisit(it.item.url, it.item.visitedAt)
                        reloadData()
                    }
                    is HistoryAction.Delete.Some -> launch(Dispatchers.IO) {
                        it.items.forEach { item ->
                            requireComponents.core.historyStorage.deleteVisit(item.url, item.visitedAt)
                        }
                        reloadData()
                    }
                    is HistoryAction.SwitchMode -> activity?.invalidateOptionsMenu()
                }
            }
    }

    @Suppress("ComplexMethod")
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.share_history_multi_select -> {
                val selectedHistory = (historyComponent.uiView as HistoryUIView).getSelected()
                when {
                    selectedHistory.size == 1 -> share(selectedHistory.first().url)
                    selectedHistory.size > 1 -> {
                        val shareText = selectedHistory.joinToString("\n") {
                            it.url
                        }
                        share(shareText)
                    }
                }
                true
            }
            R.id.libraryClose -> {
                Navigation.findNavController(requireActivity(), R.id.container)
                    .popBackStack(R.id.libraryFragment, true)
                true
            }
            R.id.delete_history_multi_select -> {
                val components = context?.applicationContext?.components!!
                val selectedHistory = (historyComponent.uiView as HistoryUIView).getSelected()

                CoroutineScope(Main).launch {
                    deleteSelectedHistory(selectedHistory, components)
                    reloadData()
                }
                true
            }
            R.id.open_history_in_new_tabs_multi_select -> {
                val selectedHistory = (historyComponent.uiView as HistoryUIView).getSelected()
                selectedHistory.forEach {
                    requireComponents.useCases.tabsUseCases.addTab.invoke(it.url)
                }

                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Normal
                (activity as HomeActivity).supportActionBar?.hide()
                navigation.navigate(HistoryFragmentDirections.actionHistoryFragmentToHomeFragment())
                true
            }
            R.id.open_history_in_private_tabs_multi_select -> {
                val selectedHistory = (historyComponent.uiView as HistoryUIView).getSelected()
                selectedHistory.forEach {
                    requireComponents.useCases.tabsUseCases.addPrivateTab.invoke(it.url)
                }

                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Private
                (activity as HomeActivity).supportActionBar?.hide()
                navigation.navigate(HistoryFragmentDirections.actionHistoryFragmentToHomeFragment())
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onBackPressed(): Boolean = (historyComponent.uiView as HistoryUIView).onBackPressed()

    private suspend fun reloadData() {
        val excludeTypes = listOf(
            VisitType.NOT_A_VISIT,
            VisitType.DOWNLOAD,
            VisitType.REDIRECT_TEMPORARY,
            VisitType.RELOAD,
            VisitType.EMBED,
            VisitType.FRAMED_LINK,
            VisitType.REDIRECT_PERMANENT
        )
        // Until we have proper pagination, only display a limited set of history to avoid blowing up the UI.
        // See https://github.com/mozilla-mobile/fenix/issues/1393
        @SuppressWarnings("MagicNumber")
        val historyCutoffMs = 1000L * 60 * 60 * 24 * 3 // past few days
        val items = requireComponents.core.historyStorage.getDetailedVisits(
            System.currentTimeMillis() - historyCutoffMs, excludeTypes = excludeTypes
        )
            // We potentially have a large amount of visits, and multiple processing steps.
            // Wrapping iterator in a sequence should make this a little more efficient.
            .asSequence()
            .sortedByDescending { it.visitTime }

            .mapIndexed { id, item ->
                HistoryItem(
                    id, if (TextUtils.isEmpty(item.title!!)) try {
                        URL(item.url).host
                    } catch (e: MalformedURLException) {
                        item.url
                    } else item.title!!, item.url, item.visitTime
                )
            }
            .toList()

        coroutineScope {
            launch(Dispatchers.Main) {
                getManagedEmitter<HistoryChange>().onNext(HistoryChange.Change(items))
            }
        }
    }

    private suspend fun deleteSelectedHistory(
        selected: List<HistoryItem>,
        components: Components = requireComponents
    ) {
        selected.forEach {
            components.core.historyStorage.deleteVisit(it.url, it.visitedAt)
        }
    }

    private fun share(text: String) {
        val directions = HistoryFragmentDirections.actionHistoryFragmentToShareFragment(text)
        Navigation.findNavController(view!!).navigate(directions)
    }
}
