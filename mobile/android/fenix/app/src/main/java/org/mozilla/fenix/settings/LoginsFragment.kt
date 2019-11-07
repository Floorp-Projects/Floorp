/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.annotation.TargetApi
import android.app.Activity.RESULT_OK
import android.app.KeyguardManager
import android.content.Context.KEYGUARD_SERVICE
import android.content.DialogInterface
import android.content.Intent
import android.os.Build
import android.os.Build.VERSION_CODES.M
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import java.util.concurrent.Executors

@Suppress("TooManyFunctions")
class LoginsFragment : PreferenceFragmentCompat(), AccountObserver {

    @TargetApi(M)
    private lateinit var biometricPromptCallback: BiometricPrompt.AuthenticationCallback

    @TargetApi(M)
    private val executor = Executors.newSingleThreadExecutor()

    @TargetApi(M)
    private lateinit var biometricPrompt: BiometricPrompt

    @TargetApi(M)
    private lateinit var promptInfo: BiometricPrompt.PromptInfo

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.logins_preferences, rootKey)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        biometricPromptCallback = object : BiometricPrompt.AuthenticationCallback() {

            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                // Authentication Error
            }

            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                lifecycleScope.launch(Main) {
                    navigateToSavedLoginsFragment()
                }
            }

            override fun onAuthenticationFailed() {
                // Authenticated Failed
            }
        }

        biometricPrompt = BiometricPrompt(this, executor, biometricPromptCallback)

        promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setTitle(getString(R.string.logins_biometric_prompt_message))
            .setDeviceCredentialAllowed(true)
            .build()
    }

    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preferences_passwords_logins_and_passwords)
        (activity as AppCompatActivity).supportActionBar?.show()

        val savedLoginsKey = getPreferenceKey(R.string.pref_key_saved_logins)
        findPreference<Preference>(savedLoginsKey)?.setOnPreferenceClickListener {
            if (Build.VERSION.SDK_INT >= M && isHardwareAvailable && hasBiometricEnrolled) {
                biometricPrompt.authenticate(promptInfo)
            } else {
                verifyPinOrShowSetupWarning()
            }
            true
        }

        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.register(this, owner = this)

        val accountExists = accountManager.authenticatedAccount() != null
        val needsReauth = accountManager.accountNeedsReauth()
        when {
            needsReauth -> updateSyncPreferenceNeedsReauth()
            accountExists -> updateSyncPreferenceStatus()
            !accountExists -> updateSyncPreferenceNeedsLogin()
        }
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) =
        updateSyncPreferenceStatus()

    override fun onLoggedOut() = updateSyncPreferenceNeedsLogin()

    override fun onAuthenticationProblems() = updateSyncPreferenceNeedsReauth()

    val isHardwareAvailable: Boolean by lazy {
        if (Build.VERSION.SDK_INT >= M) {
            context?.let {
                val bm = BiometricManager.from(it)
                val canAuthenticate = bm.canAuthenticate()
                !(canAuthenticate == BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE ||
                        canAuthenticate == BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE)
            } ?: false
        } else {
            false
        }
    }

    val hasBiometricEnrolled: Boolean by lazy {
        if (Build.VERSION.SDK_INT >= M) {
            context?.let {
                val bm = BiometricManager.from(it)
                val canAuthenticate = bm.canAuthenticate()
                (canAuthenticate == BiometricManager.BIOMETRIC_SUCCESS)
            } ?: false
        } else {
            false
        }
    }

    private fun updateSyncPreferenceStatus() {
        val syncLogins = getPreferenceKey(R.string.pref_key_password_sync_logins)
        findPreference<Preference>(syncLogins)?.apply {
            val syncEnginesStatus = SyncEnginesStorage(context!!).getStatus()
            val loginsSyncStatus = syncEnginesStatus.getOrElse(SyncEngine.Passwords) { false }
            summary = getString(
                if (loginsSyncStatus) R.string.preferences_passwords_sync_logins_on
                else R.string.preferences_passwords_sync_logins_off
            )
            setOnPreferenceClickListener {
                navigateToAccountSettingsFragment()
                true
            }
        }
    }

    private fun updateSyncPreferenceNeedsLogin() {
        val syncLogins = getPreferenceKey(R.string.pref_key_password_sync_logins)
        findPreference<Preference>(syncLogins)?.apply {
            summary = getString(R.string.preferences_passwords_sync_logins_sign_in)
            setOnPreferenceClickListener {
                navigateToTurnOnSyncFragment()
                true
            }
        }
    }

    private fun updateSyncPreferenceNeedsReauth() {
        val syncLogins = getPreferenceKey(R.string.pref_key_password_sync_logins)
        findPreference<Preference>(syncLogins)?.apply {
            summary = getString(R.string.preferences_passwords_sync_logins_reconnect)
            setOnPreferenceClickListener {
                navigateToAccountProblemFragment()
                true
            }
        }
    }

    private fun verifyPinOrShowSetupWarning() {
        val manager = activity?.getSystemService(KEYGUARD_SERVICE) as KeyguardManager
        if (manager.isKeyguardSecure) {
            showPinVerification()
        } else {
            if (context?.settings()?.shouldShowSecurityPinWarning == true) {
                showPinDialogWarning()
            } else {
                navigateToSavedLoginsFragment()
            }
        }
    }

    private fun showPinDialogWarning() {
        context?.let {
            AlertDialog.Builder(it).apply {
                setTitle(getString(R.string.logins_warning_dialog_title))
                setMessage(
                    getString(R.string.logins_warning_dialog_message)
                )

                setNegativeButton(getString(R.string.logins_warning_dialog_later)) { _: DialogInterface, _ ->
                    navigateToSavedLoginsFragment()
                }

                setPositiveButton(getString(R.string.logins_warning_dialog_set_up_now)) { it: DialogInterface, _ ->
                    it.dismiss()
                    val intent = Intent(
                        android.provider.Settings.ACTION_SECURITY_SETTINGS
                    )
                    startActivity(intent)
                }
                create()
            }.show()
            it.settings().incrementShowLoginsSecureWarningCount()
        }
    }

    private fun showPinVerification() {
        val manager = activity?.getSystemService(KEYGUARD_SERVICE) as KeyguardManager
        val intent = manager.createConfirmDeviceCredentialIntent(
            getString(R.string.logins_biometric_prompt_message_pin),
            getString(R.string.logins_biometric_prompt_message)
        )
        startActivityForResult(intent, PIN_REQUEST)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == PIN_REQUEST && resultCode == RESULT_OK) {
            navigateToSavedLoginsFragment()
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    private fun navigateToSavedLoginsFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToSavedLoginsFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToAccountSettingsFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToAccountSettingsFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToAccountProblemFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToAccountProblemFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToTurnOnSyncFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToTurnOnSyncFragment()
        findNavController().navigate(directions)
    }

    companion object {
        const val PIN_REQUEST = 303
    }
}
