/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.auth

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.biometric.BiometricManager.Authenticators.BIOMETRIC_WEAK
import androidx.biometric.BiometricManager.Authenticators.DEVICE_CREDENTIAL
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger

/**
 * A [LifecycleAwareFeature] for the Android Biometric API to prompt for user authentication.
 * The prompt also requests support for the device PIN as a fallback authentication mechanism.
 *
 * @param context Android context.
 * @param fragment The fragment on which this feature will live.
 * @param authenticationDelegate Callbacks for BiometricPrompt.
 */
class BiometricPromptAuth(
    private val context: Context,
    private val fragment: Fragment,
    private val authenticationDelegate: AuthenticationDelegate
) : LifecycleAwareFeature {
    private val logger = Logger(javaClass.simpleName)

    @VisibleForTesting
    internal var biometricPrompt: BiometricPrompt? = null

    override fun start() {
        val executor = ContextCompat.getMainExecutor(context)
        biometricPrompt = BiometricPrompt(fragment, executor, PromptCallback())
    }

    override fun stop() {
        biometricPrompt = null
    }

    /**
     * Requests the user for biometric authentication.
     *
     * @param title Adds a title for the authentication prompt.
     * @param subtitle Adds a subtitle for the authentication prompt.
     */
    fun requestAuthentication(
        title: String,
        subtitle: String = ""
    ) {
        val promptInfo: BiometricPrompt.PromptInfo = BiometricPrompt.PromptInfo.Builder()
            .setAllowedAuthenticators(BIOMETRIC_WEAK or DEVICE_CREDENTIAL)
            .setTitle(title)
            .setSubtitle(subtitle)
            .build()
        biometricPrompt?.authenticate(promptInfo)
    }

    internal inner class PromptCallback : BiometricPrompt.AuthenticationCallback() {
        override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
            logger.error("onAuthenticationError: errorMessage $errString errorCode=$errorCode")
            authenticationDelegate.onAuthError(errString.toString())
        }

        override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
            logger.debug("onAuthenticationSucceeded")
            authenticationDelegate.onAuthSuccess()
        }

        override fun onAuthenticationFailed() {
            logger.error("onAuthenticationFailed")
            authenticationDelegate.onAuthFailure()
        }
    }
}
