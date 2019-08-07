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
import kotlinx.android.synthetic.main.fragment_exceptions.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.settings.SupportUtils

class ExceptionsFragment : Fragment() {
    private lateinit var exceptionsStore: ExceptionsStore
    private lateinit var exceptionsView: ExceptionsView
    private lateinit var exceptionsInteractor: ExceptionsInteractor

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
        exceptionsStore = StoreProvider.get(this) {
            ExceptionsStore(
                ExceptionsState(
                    items = loadAndMapExceptions()
                )
            )
        }
        exceptionsInteractor =
            ExceptionsInteractor(::openLearnMore, ::deleteOneItem, ::deleteAllItems)
        exceptionsView = ExceptionsView(view.exceptionsLayout, exceptionsInteractor)
        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(exceptionsStore) {
            exceptionsView.update(it)
        }
    }

    private fun deleteAllItems() {
        viewLifecycleOwner.lifecycleScope.launch(IO) {
            val domains = ExceptionDomains.load(context!!)
            ExceptionDomains.remove(context!!, domains)
            reloadData()
        }
    }

    private fun deleteOneItem(item: ExceptionsItem) {
        viewLifecycleOwner.lifecycleScope.launch(IO) {
            ExceptionDomains.remove(context!!, listOf(item.url))
            reloadData()
        }
    }

    private fun openLearnMore() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                (SupportUtils.SumoTopic.TRACKING_PROTECTION),
            newTab = true,
            from = BrowserDirection.FromExceptions
        )
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
                exceptionsStore.dispatch(ExceptionsAction.Change(items))
            }
        }
    }
}
