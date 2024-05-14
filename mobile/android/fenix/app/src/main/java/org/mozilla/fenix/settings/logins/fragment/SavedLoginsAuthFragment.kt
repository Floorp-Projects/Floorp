/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.content.Intent
import android.os.Bundle
import androidx.activity.result.ActivityResultLauncher
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import mozilla.components.feature.autofill.preference.AutofillPreference
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.navigateWithBreadcrumb
import org.mozilla.fenix.ext.registerForActivityResult
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SharedPreferenceUpdater
import org.mozilla.fenix.settings.SyncPreferenceView
import org.mozilla.fenix.settings.biometric.bindBiometricsCredentialsPromptOrShowWarning
import org.mozilla.fenix.settings.requirePreference

@Suppress("TooManyFunctions")
class SavedLoginsAuthFragment : PreferenceFragmentCompat() {
    private lateinit var savedLoginsFragmentLauncher: ActivityResultLauncher<Intent>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        savedLoginsFragmentLauncher = registerForActivityResult {
            navigateToSavedLoginsFragment()
        }
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.logins_preferences, rootKey)
    }

    /**
     * There is a bug where while the biometric prompt is showing, you were able to quickly navigate
     * so we are disabling the settings that navigate while authenticating.
     * https://github.com/mozilla-mobile/fenix/issues/12312
     */
    private fun togglePrefsEnabledWhileAuthenticating(enabled: Boolean) {
        requirePreference<Preference>(R.string.pref_key_sync_logins).isEnabled = enabled
        requirePreference<Preference>(R.string.pref_key_save_logins_settings).isEnabled = enabled
        requirePreference<Preference>(R.string.pref_key_saved_logins).isEnabled = enabled
    }

    @Suppress("LongMethod")
    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_passwords_logins_and_passwords_2))

        requirePreference<Preference>(R.string.pref_key_save_logins_settings).apply {
            summary = getString(
                if (context.settings().shouldPromptToSaveLogins) {
                    R.string.preferences_passwords_save_logins_ask_to_save
                } else {
                    R.string.preferences_passwords_save_logins_never_save
                },
            )
            setOnPreferenceClickListener {
                navigateToSaveLoginSettingFragment()
                true
            }
        }

        requirePreference<AutofillPreference>(R.string.pref_key_android_autofill).apply {
            update()
        }

        requirePreference<Preference>(R.string.pref_key_login_exceptions).apply {
            setOnPreferenceClickListener {
                navigateToLoginExceptionFragment()
                true
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_autofill_logins).apply {
            title = context.getString(
                R.string.preferences_passwords_autofill2,
                getString(R.string.app_name),
            )
            summary = context.getString(
                R.string.preferences_passwords_autofill_description,
                getString(R.string.app_name),
            )
            isChecked = context.settings().shouldAutofillLogins
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    context.components.core.engine.settings.loginAutofillEnabled =
                        newValue as Boolean
                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<Preference>(R.string.pref_key_saved_logins).setOnPreferenceClickListener {
            view?.let { view ->
                bindBiometricsCredentialsPromptOrShowWarning(
                    view = view,
                    onShowPinVerification = { intent ->
                        savedLoginsFragmentLauncher.launch(intent)
                    },
                    onAuthSuccess = ::navigateToSavedLoginsFragment,
                    onAuthFailure = { togglePrefsEnabledWhileAuthenticating(true) },
                    doWhileAuthenticating = { togglePrefsEnabledWhileAuthenticating(false) },
                )
            }
            true
        }

        SyncPreferenceView(
            syncPreference = requirePreference(R.string.pref_key_sync_logins),
            lifecycleOwner = viewLifecycleOwner,
            accountManager = requireComponents.backgroundServices.accountManager,
            syncEngine = SyncEngine.Passwords,
            loggedOffTitle = requireContext()
                .getString(R.string.preferences_passwords_sync_logins_across_devices_2),
            loggedInTitle = requireContext()
                .getString(R.string.preferences_passwords_sync_logins_2),
            onSyncSignInClicked = {
                val directions =
                    SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment(
                        entrypoint = FenixFxAEntryPoint.SavedLogins,
                    )
                findNavController().navigate(directions)
            },
            onReconnectClicked = {
                val directions =
                    SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment(
                        entrypoint = FenixFxAEntryPoint.SavedLogins,
                    )
                findNavController().navigate(directions)
            },
        )

        togglePrefsEnabledWhileAuthenticating(true)
    }

    /**
     * Called when authentication succeeds.
     */
    private fun navigateToSavedLoginsFragment() {
        if (findNavController().currentDestination?.id == R.id.savedLoginsAuthFragment) {
            Logins.openLogins.record(NoExtras())
            val directions =
                SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToLoginsListFragment()
            findNavController().navigate(directions)
        }
    }

    private fun navigateToSaveLoginSettingFragment() {
        val directions =
            SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToSavedLoginsSettingFragment()
        findNavController().navigateWithBreadcrumb(
            directions = directions,
            navigateFrom = "SavedLoginsAuthFragment",
            navigateTo = "ActionSavedLoginsAuthFragmentToSavedLoginsSettingFragment",
            crashReporter = requireComponents.analytics.crashReporter,
        )
    }

    private fun navigateToLoginExceptionFragment() {
        val directions =
            SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToLoginExceptionsFragment()
        findNavController().navigate(directions)
    }
}
