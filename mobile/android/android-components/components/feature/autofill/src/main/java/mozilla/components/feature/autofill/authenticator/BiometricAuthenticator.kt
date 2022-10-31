/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.authenticator

import android.content.Context
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R

private const val AUTHENTICATORS =
    BiometricManager.Authenticators.BIOMETRIC_WEAK or BiometricManager.Authenticators.DEVICE_CREDENTIAL

/**
 * [Authenticator] implementation that uses [BiometricManager] and [BiometricPrompt] to authorize
 * the user.
 */
internal class BiometricAuthenticator(
    private val configuration: AutofillConfiguration,
) : Authenticator {

    override fun prompt(activity: FragmentActivity, callback: Authenticator.Callback) {
        val executor = ContextCompat.getMainExecutor(activity)
        val biometricPrompt = BiometricPrompt(activity, executor, PromptCallback(callback))

        val promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setAllowedAuthenticators(AUTHENTICATORS)
            .setTitle(
                activity.getString(
                    R.string.mozac_feature_autofill_popup_unlock_application,
                    configuration.applicationName,
                ),
            )
            .build()

        biometricPrompt.authenticate(promptInfo)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int) = Unit

    companion object {
        /**
         * Returns `true` if biometric authentication with [BiometricAuthenticator] is possible.
         */
        fun isAvailable(context: Context): Boolean {
            val biometricManager = BiometricManager.from(context)
            return biometricManager.canAuthenticate(AUTHENTICATORS) ==
                BiometricManager.BIOMETRIC_SUCCESS
        }

        /**
         * Returns `true` if biometric authentication with [BiometricAuthenticator] is not possible
         * yet, but the user can enroll and create credentials for it.
         */
        fun canEnroll(context: Context): Boolean {
            val biometricManager = BiometricManager.from(context)
            return biometricManager.canAuthenticate(AUTHENTICATORS) ==
                BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED
        }
    }
}

private class PromptCallback(
    private val callback: Authenticator.Callback,
) : BiometricPrompt.AuthenticationCallback() {
    override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
        callback.onAuthenticationSucceeded()
    }

    override fun onAuthenticationFailed() {
        callback.onAuthenticationFailed()
    }

    override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
        callback.onAuthenticationError()
    }
}
