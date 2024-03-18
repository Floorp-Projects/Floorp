/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.authenticator

import android.content.Context
import androidx.fragment.app.FragmentActivity
import mozilla.components.feature.autofill.AutofillConfiguration

/**
 * Shared interface to support multiple authentication methods.
 */
internal interface Authenticator {
    /**
     * Shows an authentication prompt and will invoke [callback] once authentication succeeded or
     * failed.
     */
    fun prompt(activity: FragmentActivity, callback: Callback)

    /**
     * For passing an activity launch result to the authenticator.
     */
    fun onActivityResult(requestCode: Int, resultCode: Int)

    /**
     * Callback getting invoked by an [Authenticator] implementation once authentication completed.
     */
    interface Callback {
        /**
         * Called when a biometric (e.g. fingerprint, face, etc.) is recognized, indicating that the
         * user has successfully authenticated.
         */
        fun onAuthenticationSucceeded()

        /**
         * Called when a biometric (e.g. fingerprint, face, etc.) is presented but not recognized as
         * belonging to the user.
         */
        fun onAuthenticationFailed()

        /**
         * Called when an unrecoverable error has been encountered and authentication has stopped.
         */
        fun onAuthenticationError()
    }
}

/**
 * Creates an [Authenticator] for the current device setup.
 */
internal fun createAuthenticator(
    context: Context,
    configuration: AutofillConfiguration,
): Authenticator? {
    return when {
        BiometricAuthenticator.isAvailable(context) -> BiometricAuthenticator(configuration)
        DeviceCredentialAuthenticator.isAvailable(context) -> DeviceCredentialAuthenticator(configuration)
        else -> null
    }
}
