/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.app.Activity.RESULT_OK
import android.app.KeyguardManager
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.provider.Settings.ACTION_SECURITY_SETTINGS
import android.view.View
import androidx.appcompat.app.AlertDialog
import androidx.core.content.getSystemService
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.secure
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SharedPreferenceUpdater
import org.mozilla.fenix.settings.SyncPreferenceView
import org.mozilla.fenix.settings.biometric.BiometricPromptFeature
import org.mozilla.fenix.settings.requirePreference

@Suppress("TooManyFunctions")
class SavedLoginsAuthFragment : PreferenceFragmentCompat() {

    private val biometricPromptFeature = ViewBoundFeatureWrapper<BiometricPromptFeature>()

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

    private fun navigateToSavedLogins() {
        runIfFragmentIsAttached {
            viewLifecycleOwner.lifecycleScope.launch(Main) {
                // Workaround for likely biometric library bug
                // https://github.com/mozilla-mobile/fenix/issues/8438
                delay(SHORT_DELAY_MS)
                navigateToSavedLoginsFragment()
            }
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        biometricPromptFeature.set(
            feature = BiometricPromptFeature(
                context = requireContext(),
                fragment = this,
                onAuthFailure = { togglePrefsEnabledWhileAuthenticating(true) },
                onAuthSuccess = ::navigateToSavedLogins
            ),
            owner = this,
            view = view
        )
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_passwords_logins_and_passwords))

        requirePreference<Preference>(R.string.pref_key_save_logins_settings).apply {
            summary = getString(
                if (context.settings().shouldPromptToSaveLogins)
                    R.string.preferences_passwords_save_logins_ask_to_save else
                    R.string.preferences_passwords_save_logins_never_save
            )
            setOnPreferenceClickListener {
                navigateToSaveLoginSettingFragment()
                true
            }
        }

        requirePreference<Preference>(R.string.pref_key_login_exceptions).apply {
            setOnPreferenceClickListener {
                navigateToLoginExceptionFragment()
                true
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_autofill_logins).apply {
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
            verifyCredentialsOrShowSetupWarning(it.context)
            true
        }

        SyncPreferenceView(
            syncPreference = requirePreference(R.string.pref_key_sync_logins),
            lifecycleOwner = viewLifecycleOwner,
            accountManager = requireComponents.backgroundServices.accountManager,
            syncEngine = SyncEngine.Passwords,
            loggedOffTitle = requireContext()
                .getString(R.string.preferences_passwords_sync_logins_across_devices),
            loggedInTitle = requireContext()
                .getString(R.string.preferences_passwords_sync_logins),
            onSignInToSyncClicked = {
                val directions =
                    SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment()
                findNavController().navigate(directions)
            },
            onReconnectClicked = {
                val directions =
                    SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment()
                findNavController().navigate(directions)
            }
        )

        togglePrefsEnabledWhileAuthenticating(true)
    }

    private fun verifyCredentialsOrShowSetupWarning(context: Context) {
        // Use the BiometricPrompt first
        if (BiometricPromptFeature.canUseFeature(context)) {
            togglePrefsEnabledWhileAuthenticating(false)
            biometricPromptFeature.get()
                ?.requestAuthentication(getString(R.string.logins_biometric_prompt_message))
            return
        }

        // Fallback to prompting for password with the KeyguardManager
        val manager = context.getSystemService<KeyguardManager>()
        if (manager?.isKeyguardSecure == true) {
            showPinVerification(manager)
        } else {
            // Warn that the device has not been secured
            if (context.settings().shouldShowSecurityPinWarning) {
                showPinDialogWarning(context)
            } else {
                navigateToSavedLoginsFragment()
            }
        }
    }

    private fun showPinDialogWarning(context: Context) {
        AlertDialog.Builder(context).apply {
            setTitle(getString(R.string.logins_warning_dialog_title))
            setMessage(
                getString(R.string.logins_warning_dialog_message)
            )

            setNegativeButton(getString(R.string.logins_warning_dialog_later)) { _: DialogInterface, _ ->
                navigateToSavedLoginsFragment()
            }

            setPositiveButton(getString(R.string.logins_warning_dialog_set_up_now)) { it: DialogInterface, _ ->
                it.dismiss()
                val intent = Intent(ACTION_SECURITY_SETTINGS)
                startActivity(intent)
            }
            create()
        }.show().secure(activity)
        context.settings().incrementSecureWarningCount()
    }

    @Suppress("Deprecation") // This is only used when BiometricPrompt is unavailable
    private fun showPinVerification(manager: KeyguardManager) {
        val intent = manager.createConfirmDeviceCredentialIntent(
            getString(R.string.logins_biometric_prompt_message_pin),
            getString(R.string.logins_biometric_prompt_message)
        )
        startActivityForResult(intent, PIN_REQUEST)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == PIN_REQUEST && resultCode == RESULT_OK) {
            navigateToSavedLoginsFragment()
        }
    }

    /**
     * Called when authentication succeeds.
     */
    private fun navigateToSavedLoginsFragment() {
        context?.components?.analytics?.metrics?.track(Event.OpenLogins)
        val directions =
            SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToLoginsListFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToSaveLoginSettingFragment() {
        val directions =
            SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToSavedLoginsSettingFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToLoginExceptionFragment() {
        val directions =
            SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToLoginExceptionsFragment()
        findNavController().navigate(directions)
    }

    companion object {
        const val SHORT_DELAY_MS = 100L
        const val PIN_REQUEST = 303
    }
}
