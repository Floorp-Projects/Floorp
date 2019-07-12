/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_exceptions.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter

class ExceptionsFragment : Fragment() {
    private lateinit var exceptionsComponent: ExceptionsComponent

    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preference_exceptions)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_exceptions, container, false)
        exceptionsComponent = ExceptionsComponent(
            view.exceptions_layout,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                ExceptionsViewModel::class.java
            ) {
                ExceptionsViewModel(ExceptionsState(loadAndMapExceptions()))
            }
        )
        return view
    }

    override fun onStart() {
        super.onStart()
        getAutoDisposeObservable<ExceptionsAction>()
            .subscribe {
                when (it) {
                    is ExceptionsAction.Delete.All -> viewLifecycleOwner.lifecycleScope.launch(IO) {
                        val domains = ExceptionDomains.load(context!!)
                        ExceptionDomains.remove(context!!, domains)
                        launch(Main) {
                            view?.let { view -> Navigation.findNavController(view).navigateUp() }
                        }
                    }
                    is ExceptionsAction.Delete.One -> viewLifecycleOwner.lifecycleScope.launch(IO) {
                        ExceptionDomains.remove(context!!, listOf(it.item.url))
                        reloadData()
                    }
                }
            }
    }

    private fun loadAndMapExceptions(): List<ExceptionsItem> {
        return ExceptionDomains.load(context!!)
            .map { item ->
                ExceptionsItem(
                    item
                )
            }
    }

    private suspend fun reloadData() {
        val items = loadAndMapExceptions()

        coroutineScope {
            launch(Main) {
                if (items.isEmpty()) {
                    view?.let { view: View -> Navigation.findNavController(view).navigateUp() }
                    return@launch
                }
                getManagedEmitter<ExceptionsChange>().onNext(ExceptionsChange.Change(items))
            }
        }
    }
}
