/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.annotation.SuppressLint
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
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getSafeManagedObservable
import kotlin.coroutines.CoroutineContext

class HistoryFragment : Fragment(), CoroutineScope {

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_history, container, false)
        HistoryComponent(view.history_layout, ActionBusFactory.get(this), initialState = HistoryState(emptyList()))

        return view
    }

    @SuppressLint("CheckResult")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()

        setHasOptionsMenu(true)
        (activity as AppCompatActivity).supportActionBar?.show()

        getSafeManagedObservable<HistoryAction>()
            .subscribe {
                if (it is HistoryAction.Select) {
                    Navigation.findNavController(requireActivity(), R.id.container).apply {
                        navigate(
                            HistoryFragmentDirections.actionGlobalBrowser(null,
                                (activity as HomeActivity).browsingModeManager.isPrivate),
                            NavOptions.Builder().setPopUpTo(R.id.homeFragment, false).build()
                        )
                    }

                    requireComponents.useCases.sessionUseCases.loadUrl.invoke(it.item.url)
                }
            }
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

        val eventEmitter = ActionBusFactory.get(this)

        launch(Dispatchers.IO) {
            val items = requireComponents.core.historyStorage.getVisited().map { HistoryItem(it) }

            launch(Dispatchers.Main) {
                eventEmitter.emit(HistoryChange::class.java, HistoryChange.Change(items))
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
                // TODO Library Search
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
