/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.NavOptions
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import kotlin.coroutines.CoroutineContext

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
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    private fun selectItem(item: HistoryItem) {
        Navigation.findNavController(requireActivity(), R.id.container).apply {
            navigate(
                HistoryFragmentDirections.actionGlobalBrowser(null),
                NavOptions.Builder().setPopUpTo(R.id.homeFragment, false).build()
            )
        }

        requireComponents.useCases.sessionUseCases.loadUrl.invoke(item.url)
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
            val items = requireComponents.core.historyStorage.getVisited()
                .mapIndexed { id, item -> HistoryItem(id, item) }

            launch(Dispatchers.Main) {
                getManagedEmitter<HistoryChange>().onNext(HistoryChange.Change(items))
            }
        }
    }

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
}
