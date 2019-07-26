/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.os.Bundle
import android.text.InputFilter
import android.text.format.DateUtils
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.lib.state.ext.observe
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.fxa.FxaPanicException
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.service.fxa.sync.getLastSynced
import mozilla.components.support.base.log.logger.Logger
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
                    deviceName = ""
                )
            )
        }

        accountSettingsStore.observe(this) {
            viewLifecycleOwner.lifecycleScope.launch {
                updateLastSyncTimePref(it)
                updateDeviceName(it)
            }
        }

        accountManager = requireComponents.backgroundServices.accountManager
        accountManager.register(accountStateObserver, this, true)

        accountSettingsInteractor = AccountSettingsInteractor(
            findNavController(),
            ::onSyncNow,
            ::makeSnackbar,
            ::syncDeviceName,
            accountSettingsStore
        )

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

    private fun onSyncNow() {
        lifecycleScope.launch {
            requireComponents.analytics.metrics.track(Event.SyncAccountSyncNow)
            // Trigger a sync.
            requireComponents.backgroundServices.accountManager.syncNowAsync().await()
            // Poll for device events.
            accountManager.authenticatedAccount()
                ?.deviceConstellation()
                ?.refreshDeviceStateAsync()
                ?.await()
        }
    }

    private fun makeSnackbar(newValue: String): Boolean {
        // The network request requires a nonempty string, so don't persist any changes if the user inputs one.
        if (newValue.trim().isEmpty()) {
            FenixSnackbar.make(view!!, FenixSnackbar.LENGTH_LONG)
                .setText(getString(R.string.empty_device_name_error))
                .show()
            return false
        }
        return true
    }

    private fun syncDeviceName(newValue: String) {
        // This may fail, and we'll have a disparity in the UI until `updateDeviceName` is called.
        lifecycleScope.launch(IO) {
            try {
                accountManager.authenticatedAccount()
                    ?.deviceConstellation()
                    ?.setDeviceNameAsync(newValue)
                    ?.await()
            } catch (e: FxaPanicException) {
                throw e
            } catch (e: FxaException) {
                Logger.error("Setting device name failed.", e)
            }
        }
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
            accountSettingsInteractor.onChangeDeviceName(newValue as String)
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
