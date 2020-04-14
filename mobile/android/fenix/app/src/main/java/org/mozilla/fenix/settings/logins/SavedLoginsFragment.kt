/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import androidx.appcompat.widget.SearchView
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_saved_logins.view.*
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.Login
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.checkAndUpdateScreenshotPermission
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SupportUtils

class SavedLoginsFragment : Fragment() {
    private lateinit var savedLoginsStore: SavedLoginsFragmentStore
    private lateinit var savedLoginsView: SavedLoginsView
    private lateinit var savedLoginsInteractor: SavedLoginsInteractor

    override fun onResume() {
        super.onResume()
        activity?.window?.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
        showToolbar(getString(R.string.preferences_passwords_saved_logins))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_saved_logins, container, false)
        savedLoginsStore = StoreProvider.get(this) {
            SavedLoginsFragmentStore(
                SavedLoginsFragmentState(
                    isLoading = true,
                    items = listOf(),
                    filteredItems = listOf()
                )
            )
        }
        savedLoginsInteractor = SavedLoginsInteractor(::itemClicked, ::openLearnMore)
        savedLoginsView = SavedLoginsView(view.savedLoginsLayout, savedLoginsInteractor)
        loadAndMapLogins()
        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(savedLoginsStore) {
            savedLoginsView.update(it)
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_list, menu)
        val searchItem = menu.findItem(R.id.search)
        val searchView: SearchView = searchItem.actionView as SearchView
        searchView.imeOptions = EditorInfo.IME_ACTION_DONE
        searchView.queryHint = getString(R.string.preferences_passwords_saved_logins_search)

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?): Boolean {
                return false
            }

            override fun onQueryTextChange(newText: String?): Boolean {
                savedLoginsStore.dispatch(SavedLoginsFragmentAction.FilterLogins(newText))
                return false
            }
        })
    }

    /**
     * If we pause this fragment, we want to pop users back to reauth
     */
    override fun onPause() {
        if (findNavController().currentDestination?.id != R.id.savedLoginSiteInfoFragment) {
            activity?.let { it.checkAndUpdateScreenshotPermission(it.settings()) }
            findNavController().popBackStack(R.id.loginsFragment, false)
        }
        super.onPause()
    }

    private fun itemClicked(item: SavedLoginsItem) {
        context?.components?.analytics?.metrics?.track(Event.OpenOneLogin)
        val directions =
            SavedLoginsFragmentDirections.actionSavedLoginsFragmentToSavedLoginSiteInfoFragment(item)
        findNavController().navigate(directions)
    }

    private fun openLearnMore() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                (SupportUtils.SumoTopic.SYNC_SETUP),
            newTab = true,
            from = BrowserDirection.FromSavedLoginsFragment
        )
    }

    private fun loadAndMapLogins() {
        var deferredLogins: Deferred<List<Login>>? = null
        val fetchLoginsJob = lifecycleScope.launch(IO) {
            deferredLogins = async {
                requireContext().components.core.passwordsStorage.list()
            }
            val logins = deferredLogins?.await()
            logins?.let {
                withContext(Main) {
                    savedLoginsStore.dispatch(SavedLoginsFragmentAction.UpdateLogins(logins.map { item ->
                        SavedLoginsItem(item.origin, item.username, item.password, item.guid!!)
                    }))
                }
            }
        }
        fetchLoginsJob.invokeOnCompletion {
            if (it is CancellationException) {
                deferredLogins?.cancel()
            }
        }
    }
}
