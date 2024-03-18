/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.auth

/**
 * Callbacks for BiometricPrompt Authentication
 */
interface AuthenticationDelegate {

    /**
     * Called when a biometric (e.g. fingerprint, face, etc.)
     * is presented but not recognized as belonging to the user.
     */
    fun onAuthFailure()

    /**
     * Called when a biometric (e.g. fingerprint, face, etc.) is recognized,
     * indicating that the user has successfully authenticated.
     */
    fun onAuthSuccess()

    /**
     * Called when an unrecoverable error has been encountered and authentication has stopped.
     * @param errorText A human-readable error string that can be shown on an UI
     */
    fun onAuthError(errorText: String)
}
