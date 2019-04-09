/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.os.Bundle
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.coroutineScope
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import org.mozilla.fenix.R
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

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_history, container, false)
        historyComponent = HistoryComponent(view.history_layout, ActionBusFactory.get(this))
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

    private fun selectItem(item: HistoryItem) {
        (activity as HomeActivity).openToBrowserAndLoad(item.url, from = BrowserDirection.FromHistory)
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.library_menu, menu)
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
                    is HistoryAction.Select -> selectItem(it.item)
                    is HistoryAction.EnterEditMode -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.EnterEditMode(it.item))
                    is HistoryAction.AddItemForRemoval -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.AddItemForRemoval(it.item))
                    is HistoryAction.RemoveItemForRemoval -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.RemoveItemForRemoval(it.item))
                    is HistoryAction.BackPressed -> getManagedEmitter<HistoryChange>()
                        .onNext(HistoryChange.ExitEditMode)
                    is HistoryAction.Delete.All -> launch(Dispatchers.IO) {
                        requireComponents.core.historyStorage.deleteEverything()
                        reloadData()
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
                }
            }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.libraryClose -> {
                Navigation.findNavController(requireActivity(), R.id.container)
                    .popBackStack(R.id.libraryFragment, true)
                true
            }
            R.id.librarySearch -> {
                // TODO Library Search #1118
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1118")
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onBackPressed(): Boolean = (historyComponent.uiView as HistoryUIView).onBackPressed()

    private suspend fun reloadData() {
        val allowedVisitTypes = listOf(VisitType.LINK, VisitType.TYPED, VisitType.BOOKMARK)
        // Until we have proper pagination, only display a limited set of history to avoid blowing up the UI.
        // See https://github.com/mozilla-mobile/fenix/issues/1393
        @SuppressWarnings("MagicNumber")
        val historyCutoffMs = 1000L * 60 * 60 * 24 * 3 // past few days
        val items = requireComponents.core.historyStorage.getDetailedVisits(
            System.currentTimeMillis() - historyCutoffMs
        )
            // We potentially have a large amount of visits, and multiple processing steps.
            // Wrapping iterator in a sequence should make this a little more efficient.
            .asSequence()
            .sortedByDescending { it.visitTime }

            // Temporary filtering until we can do it at the API level.
            // See https://github.com/mozilla-mobile/android-components/issues/2643
            .filter { allowedVisitTypes.contains(it.visitType) }

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
}
