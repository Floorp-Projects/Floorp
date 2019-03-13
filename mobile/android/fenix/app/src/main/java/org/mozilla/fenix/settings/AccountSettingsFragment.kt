/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.os.Bundle
import android.text.format.DateUtils
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.feature.sync.getLastSynced
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import java.lang.Exception
import kotlin.coroutines.CoroutineContext

class AccountSettingsFragment : PreferenceFragmentCompat(), CoroutineScope {
    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
        (activity as AppCompatActivity).title = getString(R.string.preferences_account_settings)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.account_settings_preferences, rootKey)

        // Sign out
        val signOut = context!!.getPreferenceKey(R.string.pref_key_sign_out)
        val preferenceSignOut = findPreference<Preference>(signOut)
        preferenceSignOut?.onPreferenceClickListener = getClickListenerForSignOut()

        // Sync now
        val syncNow = context!!.getPreferenceKey(R.string.pref_key_sync_now)
        val preferenceSyncNow = findPreference<Preference>(syncNow)
        preferenceSyncNow?.let {
            preferenceSyncNow.onPreferenceClickListener = getClickListenerForSyncNow()

            // Current sync state
            updateLastSyncedTimePref(context!!, preferenceSyncNow)
            if (requireComponents.backgroundServices.syncManager.isSyncRunning()) {
                preferenceSyncNow.title = getString(R.string.sync_syncing)
                preferenceSyncNow.isEnabled = false
            } else {
                preferenceSyncNow.isEnabled = true
            }
        }

        // NB: ObserverRegistry will take care of cleaning up internal references to 'observer' and
        // 'owner' when appropriate.
        requireComponents.backgroundServices.syncManager.register(syncStatusObserver, owner = this, autoPause = true)
    }

    private fun getClickListenerForSignOut(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            launch {
                requireComponents.backgroundServices.accountManager.logoutAsync().await()
                Navigation.findNavController(view!!).popBackStack()
            }
            true
        }
    }

    private fun getClickListenerForSyncNow(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            requireComponents.backgroundServices.syncManager.syncNow()
            true
        }
    }

    private val syncStatusObserver = object : SyncStatusObserver {
        override fun onStarted() {
            CoroutineScope(Dispatchers.Main).launch {
                val pref = findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_now))

                pref?.title = getString(R.string.sync_syncing)
                pref?.isEnabled = false
            }
        }

        // Sync stopped successfully.
        override fun onIdle() {
            CoroutineScope(Dispatchers.Main).launch {
                val pref = findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_now))
                pref?.let {
                    pref.title = getString(R.string.preferences_sync_now)
                    pref.isEnabled = true
                    updateLastSyncedTimePref(context!!, pref, failed = false)
                }
            }
        }

        // Sync stopped after encountering a problem.
        override fun onError(error: Exception?) {
            CoroutineScope(Dispatchers.Main).launch {
                val pref = findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_now))
                pref?.let {
                    pref.title = getString(R.string.preferences_sync_now)
                    pref.isEnabled = true
                    updateLastSyncedTimePref(context!!, pref, failed = true)
                }
            }
        }
    }

    fun updateLastSyncedTimePref(context: Context, pref: Preference, failed: Boolean = false) {
        val lastSyncTime = getLastSynced(context)

        pref.summary = if (!failed && lastSyncTime == 0L) {
            // Never tried to sync.
            getString(R.string.sync_never_synced_summary)
        } else if (failed && lastSyncTime == 0L) {
            // Failed to sync, never succeeded before.
            getString(R.string.sync_failed_never_synced_summary)
        } else if (!failed && lastSyncTime != 0L) {
            // Successfully synced.
            getString(
                R.string.sync_last_synced_summary,
                DateUtils.getRelativeTimeSpanString(lastSyncTime)
            )
        } else {
            // Failed to sync, succeeded before.
            getString(
                R.string.sync_failed_summary,
                DateUtils.getRelativeTimeSpanString(lastSyncTime)
            )
        }
    }
}
