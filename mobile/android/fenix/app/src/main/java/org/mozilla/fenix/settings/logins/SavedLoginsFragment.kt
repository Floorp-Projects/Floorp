/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_saved_logins.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar

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

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_saved_logins, container, false)
        savedLoginsStore = StoreProvider.get(this) {
            SavedLoginsFragmentStore(
                SavedLoginsFragmentState(
                    items = listOf()
                )
            )
        }
        savedLoginsInteractor = SavedLoginsInteractor(::itemClicked)
        savedLoginsView = SavedLoginsView(view.savedLoginsLayout, savedLoginsInteractor)
        lifecycleScope.launch(Main) { loadAndMapLogins() }
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

    /**
     * If we pause this fragment, we want to pop users back to reauth
     */
    override fun onPause() {
        if (findNavController().currentDestination?.id != R.id.savedLoginSiteInfoFragment) {
            activity?.window?.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
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

    private suspend fun loadAndMapLogins() {
        val syncedLogins = withContext(IO) {
            requireContext().components.core.syncablePasswordsStorage.withUnlocked {
                it.list().await().map { item ->
                    SavedLoginsItem(item.hostname, item.username, item.password)
                }
            }
        }
        withContext(Main) {
            savedLoginsStore.dispatch(SavedLoginsFragmentAction.UpdateLogins(syncedLogins))
        }
    }
}
