/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.DialogInterface
import android.os.Bundle
import android.util.Log
import android.view.ContextThemeWrapper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_delete_browsing_data.*
import kotlinx.android.synthetic.main.fragment_delete_browsing_data.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
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
        view?.browsing_data_item?.onCheckListener = { _ -> updateDeleteButton() }
        view?.collections_item?.onCheckListener = { _ -> updateDeleteButton() }

        view?.delete_data?.setOnClickListener {
            askToDelete()
        }
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

    private fun askToDelete() {
        AlertDialog.Builder(
            ContextThemeWrapper(
                activity,
                R.style.DialogStyle
            )
        ).apply {
            val appName = context.getString(R.string.app_name)
            val message = context.getString(R.string.preferences_delete_browsing_data_prompt_message, appName)
            setMessage(message)

            setNegativeButton(R.string.preferences_delete_browsing_data_prompt_cancel) { dialog: DialogInterface, _ ->
                dialog.cancel()
            }

            setPositiveButton(R.string.preferences_delete_browsing_data_prompt_allow) { dialog: DialogInterface, _ ->
                dialog.dismiss()
                deleteSelected()
            }
            create()
        }.show()
    }

    private fun deleteSelected() {
        val open_tabs = view!!.open_tabs_item!!.isChecked
        val browsing_data = view!!.browsing_data_item!!.isChecked
        val collections = view!!.collections_item!!.isChecked

        startDeletion()
        launch(Dispatchers.IO) {
            var jobs = mutableListOf<Deferred<Unit>>()
            if (open_tabs) jobs.add(deleteTabsAsync())
            if (browsing_data) jobs.add(deleteBrowsingDataAsync())
            if (collections) jobs.add(deleteCollectionsAsync())

            jobs.awaitAll()

            launch(Dispatchers.Main) {
                finishDeletion()
            }
        }

    }

    fun startDeletion() {
        progress_bar.visibility = View.VISIBLE
        delete_browsing_data_wrapper.isEnabled = false
        delete_browsing_data_wrapper.isClickable = false
        delete_browsing_data_wrapper.alpha = 0.6f
    }

    fun finishDeletion() {
        progress_bar.visibility = View.GONE
        delete_browsing_data_wrapper.isEnabled = true
        delete_browsing_data_wrapper.isClickable = true
        delete_browsing_data_wrapper.alpha = 1f

        updateTabCount()
        updateHistoryCount()
        updateCollectionsCount()
    }

    private fun updateDeleteButton() {
        val open_tabs = view!!.open_tabs_item!!.isChecked
        val browsing_data = view!!.browsing_data_item!!.isChecked
        val collections = view!!.collections_item!!.isChecked
        val enabled = open_tabs || browsing_data || collections

        view?.delete_data?.isEnabled = enabled
        view?.delete_data?.alpha = if (enabled) 1f else 0.6f
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

    private fun deleteTabsAsync() = async(Dispatchers.IO) { requireComponents.core.sessionManager.removeSessions() }

    private fun deleteBrowsingDataAsync() = async(Dispatchers.IO) {
        requireComponents.core.engine.clearData(Engine.BrowsingData.all())
        requireComponents.core.historyStorage.deleteEverything()
    }

    private fun deleteCollectionsAsync() = async(Dispatchers.IO) {
        val count = requireComponents.core.tabCollectionStorage.getTabCollectionsCount()
        val data = requireComponents.core.tabCollectionStorage.getCollections(count).value?.forEach {
            requireComponents.core.tabCollectionStorage.removeCollection(it)
        }
    }
}
