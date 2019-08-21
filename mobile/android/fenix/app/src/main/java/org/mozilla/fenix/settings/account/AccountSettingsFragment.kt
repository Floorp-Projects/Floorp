/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.os.Bundle
import android.text.InputFilter
import android.text.format.DateUtils
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.service.fxa.sync.getLastSynced
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents

@SuppressWarnings("TooManyFunctions")
class AccountSettingsFragment : PreferenceFragmentCompat() {
    private lateinit var accountManager: FxaAccountManager
    private lateinit var accountSettingsStore: AccountSettingsStore
    private lateinit var accountSettingsInteractor: AccountSettingsInteractor

    // Navigate away from this fragment when we encounter auth problems or logout events.
    private val accountStateObserver = object : AccountObserver {
        override fun onAuthenticationProblems() {
            lifecycleScope.launch {
                findNavController().popBackStack()
            }
        }

        override fun onLoggedOut() {
            lifecycleScope.launch {
                findNavController().popBackStack()

                // Remove the device name when we log out.
                context?.let {
                    val deviceNameKey = it.getPreferenceKey(R.string.pref_key_sync_device_name)
                    preferenceManager.sharedPreferences.edit().remove(deviceNameKey).apply()
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_account_settings)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requireComponents.analytics.metrics.track(Event.SyncAccountOpened)
    }

    override fun onDestroy() {
        super.onDestroy()
        requireComponents.analytics.metrics.track(Event.SyncAccountClosed)
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(accountSettingsStore) {
            updateLastSyncTimePref(it)
            updateDeviceName(it)
        }

        accountSettingsInteractor = AccountSettingsInteractor(
            findNavController(),
            ::syncNow,
            ::syncDeviceName,
            accountSettingsStore
        )
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.account_settings_preferences, rootKey)

        accountSettingsStore = StoreProvider.get(this) {
            AccountSettingsStore(
                AccountSettingsState(
                    lastSyncedDate =
                    if (getLastSynced(requireContext()) == 0L)
                        LastSyncTime.Never
                    else
                        LastSyncTime.Success(getLastSynced(requireContext())),
                    deviceName = requireComponents.backgroundServices.defaultDeviceName(requireContext())
                )
            )
        }

        accountManager = requireComponents.backgroundServices.accountManager
        accountManager.register(accountStateObserver, this, true)

        // Sign out
        val signOut = context!!.getPreferenceKey(R.string.pref_key_sign_out)
        val preferenceSignOut = findPreference<Preference>(signOut)
        preferenceSignOut?.onPreferenceClickListener = getClickListenerForSignOut()

        // Sync now
        val syncNow = context!!.getPreferenceKey(R.string.pref_key_sync_now)
        val preferenceSyncNow = findPreference<Preference>(syncNow)
        preferenceSyncNow?.let {
            it.onPreferenceClickListener = getClickListenerForSyncNow()

            // Current sync state
            if (requireComponents.backgroundServices.accountManager.isSyncActive()) {
                it.title = getString(R.string.sync_syncing_in_progress)
                it.isEnabled = false
            } else {
                it.isEnabled = true
            }
        }

        // Device Name
        val deviceConstellation = accountManager.authenticatedAccount()?.deviceConstellation()
        val deviceNameKey = context!!.getPreferenceKey(R.string.pref_key_sync_device_name)
        findPreference<EditTextPreference>(deviceNameKey)?.apply {
            onPreferenceChangeListener = getChangeListenerForDeviceName()
            deviceConstellation?.state()?.currentDevice?.let { device ->
                summary = device.displayName
                text = device.displayName
                accountSettingsStore.dispatch(AccountSettingsAction.UpdateDeviceName(device.displayName))
            }
            setOnBindEditTextListener { editText ->
                editText.filters = arrayOf(InputFilter.LengthFilter(DEVICE_NAME_MAX_LENGTH))
            }
        }

        deviceConstellation?.registerDeviceObserver(deviceConstellationObserver, owner = this, autoPause = true)

        // NB: ObserverRegistry will take care of cleaning up internal references to 'observer' and
        // 'owner' when appropriate.
        requireComponents.backgroundServices.accountManager.registerForSyncEvents(
            syncStatusObserver, owner = this, autoPause = true
        )
    }

    private fun syncNow() {
        lifecycleScope.launch {
            requireComponents.analytics.metrics.track(Event.SyncAccountSyncNow)
            // Trigger a sync.
            requireComponents.backgroundServices.accountManager.syncNowAsync().await()
            // Poll for device events & update devices.
            accountManager.authenticatedAccount()
                ?.deviceConstellation()?.run {
                    refreshDevicesAsync().await()
                    pollForEventsAsync().await()
                }
        }
    }

    private fun syncDeviceName(newValue: String): Boolean {
        if (newValue.trim().isEmpty()) {
            return false
        }
        // This may fail, and we'll have a disparity in the UI until `updateDeviceName` is called.
        lifecycleScope.launch(Main) {
            accountManager.authenticatedAccount()
                ?.deviceConstellation()
                ?.setDeviceNameAsync(newValue)
                ?.await()
        }
        return true
    }

    private fun getClickListenerForSignOut(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            accountSettingsInteractor.onSignOut()
            true
        }
    }

    private fun getClickListenerForSyncNow(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            accountSettingsInteractor.onSyncNow()
            true
        }
    }

    private fun getChangeListenerForDeviceName(): Preference.OnPreferenceChangeListener {
        return Preference.OnPreferenceChangeListener { _, newValue ->
            accountSettingsInteractor.onChangeDeviceName(newValue as String) {
                FenixSnackbar.make(view!!, FenixSnackbar.LENGTH_LONG)
                .setText(getString(R.string.empty_device_name_error))
                .show()
            }
        }
    }

    private val syncStatusObserver = object : SyncStatusObserver {
        override fun onStarted() {
            lifecycleScope.launch {
                val pref = findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_now))
                view?.announceForAccessibility(getString(R.string.sync_syncing_in_progress))
                pref?.title = getString(R.string.sync_syncing_in_progress)
                pref?.isEnabled = false
            }
        }

        // Sync stopped successfully.
        override fun onIdle() {
            lifecycleScope.launch {
                val pref = findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_now))
                pref?.let {
                    pref.title = getString(R.string.preferences_sync_now)
                    pref.isEnabled = true

                    val time = getLastSynced(requireContext())
                    accountSettingsStore.dispatch(AccountSettingsAction.SyncEnded(time))
                }
            }
        }

        // Sync stopped after encountering a problem.
        override fun onError(error: Exception?) {
            lifecycleScope.launch {
                val pref = findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_now))
                pref?.let {
                    pref.title = getString(R.string.preferences_sync_now)
                    pref.isEnabled = true

                    val failedTime = getLastSynced(requireContext())
                    accountSettingsStore.dispatch(AccountSettingsAction.SyncFailed(failedTime))
                }
            }
        }
    }

    private val deviceConstellationObserver = object : DeviceConstellationObserver {
        override fun onDevicesUpdate(constellation: ConstellationState) {
            constellation.currentDevice?.displayName?.also {
                accountSettingsStore.dispatch(AccountSettingsAction.UpdateDeviceName(it))
            }
        }
    }

    private fun updateDeviceName(state: AccountSettingsState) {
        val deviceNameKey = context!!.getPreferenceKey(R.string.pref_key_sync_device_name)
        val preferenceDeviceName = findPreference<Preference>(deviceNameKey)
        preferenceDeviceName?.summary = state.deviceName
    }

    private fun updateLastSyncTimePref(state: AccountSettingsState) {
        val value = when (state.lastSyncedDate) {
            LastSyncTime.Never -> getString(R.string.sync_never_synced_summary)
            is LastSyncTime.Failed -> {
                if (state.lastSyncedDate.lastSync == 0L) {
                    getString(R.string.sync_failed_never_synced_summary)
                } else {
                    getString(
                        R.string.sync_failed_summary,
                        DateUtils.getRelativeTimeSpanString(state.lastSyncedDate.lastSync)
                    )
                }
            }
            is LastSyncTime.Success -> getString(
                R.string.sync_last_synced_summary,
                DateUtils.getRelativeTimeSpanString(state.lastSyncedDate.lastSync)
            )
        }

        val syncNow = context!!.getPreferenceKey(R.string.pref_key_sync_now)
        findPreference<Preference>(syncNow)?.summary = value
    }

    companion object {
        private const val DEVICE_NAME_MAX_LENGTH = 128
    }
}
