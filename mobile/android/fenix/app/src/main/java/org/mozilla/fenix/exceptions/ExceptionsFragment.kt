/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_exceptions.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.SupportUtils

class ExceptionsFragment : Fragment() {
    private lateinit var exceptionsStore: ExceptionsFragmentStore
    private lateinit var exceptionsView: ExceptionsView
    private lateinit var exceptionsInteractor: ExceptionsInteractor
    private lateinit var trackingProtectionUseCases: TrackingProtectionUseCases

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
        trackingProtectionUseCases = TrackingProtectionUseCases(
            sessionManager = view.context.components.core.sessionManager,
            engine = view.context.components.core.engine
        )
        exceptionsStore = StoreProvider.get(this) {
            ExceptionsFragmentStore(
                ExceptionsFragmentState(
                    items = listOf()
                )
            )
        }
        exceptionsInteractor =
            ExceptionsInteractor(::openLearnMore, ::deleteOneItem, ::deleteAllItems)
        exceptionsView = ExceptionsView(view.exceptionsLayout, exceptionsInteractor)
        reloadExceptions()
        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(exceptionsStore) {
            exceptionsView.update(it)
        }
    }

    private fun deleteAllItems() {
        trackingProtectionUseCases.removeAllExceptions()
        reloadExceptions()
    }

    private fun deleteOneItem(item: ExceptionsItem) {
        // We can't currently delete one item in this Exceptions list with a URL with the GV API
        // See https://github.com/mozilla-mobile/android-components/issues/4699
        Log.e("Remove one exception", "$item")
        reloadExceptions()
    }

    private fun openLearnMore() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                (SupportUtils.SumoTopic.TRACKING_PROTECTION),
            newTab = true,
            from = BrowserDirection.FromExceptions
        )
    }

    private fun reloadExceptions() {
        trackingProtectionUseCases.fetchExceptions { resultList ->
            exceptionsStore.dispatch(ExceptionsFragmentAction.Change(resultList.map {
                ExceptionsItem(
                    it
                )
            }))
        }
    }
}
