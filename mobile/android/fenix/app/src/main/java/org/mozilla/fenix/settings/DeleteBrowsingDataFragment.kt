/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_delete_browsing_data.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import kotlin.coroutines.CoroutineContext

class DeleteBrowsingDataFragment : Fragment(), CoroutineScope {
    private lateinit var sessionObserver: SessionManager.Observer

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
        inflater.inflate(R.layout.fragment_delete_browsing_data, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        job = Job()

        sessionObserver = object : SessionManager.Observer {
            override fun onSessionAdded(session: Session) {
                super.onSessionAdded(session)
                updateTabCount()
            }

            override fun onSessionRemoved(session: Session) {
                super.onSessionRemoved(session)
                updateTabCount()
            }

            override fun onSessionSelected(session: Session) {
                super.onSessionSelected(session)
                updateTabCount()
            }

            override fun onSessionsRestored() {
                super.onSessionsRestored()
                updateTabCount()
            }

            override fun onAllSessionsRemoved() {
                super.onAllSessionsRemoved()
                updateTabCount()
            }
        }

        requireComponents.core.sessionManager.register(sessionObserver, owner = this)

        view?.open_tabs_item?.onCheckListener = { _ -> updateDeleteButton() }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        job.cancel()
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).apply {
            title = getString(R.string.preferences_delete_browsing_data)
            supportActionBar?.show()
        }

        updateTabCount()
        updateHistoryCount()
        updateCollectionsCount()
        updateDeleteButton()
    }

    private fun updateDeleteButton() {
        view?.delete_data?.isEnabled =
            view!!.open_tabs_item!!.isChecked
            || view!!.browsing_data_item!!.isChecked
            || view!!.collections_item!!.isChecked

        Log.e("wat", view?.delete_data?.isEnabled.toString())
    }

    private fun updateTabCount() {
        view?.open_tabs_item?.apply {
            val openTabs = requireComponents.core.sessionManager.size
            subtitleView.text = resources.getString(R.string.preferences_delete_browsing_data_tabs_subtitle, openTabs)
        }
    }

    private fun updateHistoryCount() {
        view?.browsing_data_item?.subtitleView?.text = ""

        launch(Dispatchers.IO) {
            val historyCount = requireComponents.core.historyStorage.getVisited().size
            launch(Dispatchers.Main) {
                view?.browsing_data_item?.apply {
                    subtitleView.text =
                        resources.getString(
                            R.string.preferences_delete_browsing_data_browsing_data_subtitle,
                            historyCount
                        )
                }
            }
        }
    }

    private fun updateCollectionsCount() {
        view?.browsing_data_item?.subtitleView?.text = ""

        launch(Dispatchers.IO) {
            val collectionsCount = requireComponents.core.tabCollectionStorage.getTabCollectionsCount()
            launch(Dispatchers.Main) {
                view?.collections_item?.apply {
                    subtitleView.text =
                        resources.getString(
                            R.string.preferences_delete_browsing_data_collections_subtitle,
                            collectionsCount
                        )
                }
            }
        }
    }
}
