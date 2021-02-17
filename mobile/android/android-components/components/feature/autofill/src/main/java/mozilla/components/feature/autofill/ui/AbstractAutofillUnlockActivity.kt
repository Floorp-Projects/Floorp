/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ui

import android.app.assist.AssistStructure
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.service.autofill.FillResponse
import android.view.autofill.AutofillManager
import androidx.annotation.RequiresApi
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.handler.FillRequestHandler

/**
 * Activity responsible for unlocking the autofill service by asking the user to verify with a
 * fingerprint or alternative device unlocking mechanism.
 */
@RequiresApi(Build.VERSION_CODES.O)
abstract class AbstractAutofillUnlockActivity : FragmentActivity() {
    abstract val configuration: AutofillConfiguration

    private var fillResponse: Deferred<FillResponse?>? = null
    private val fillHandler by lazy { FillRequestHandler(context = this, configuration) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val structure: AssistStructure? = intent.getParcelableExtra(AutofillManager.EXTRA_ASSIST_STRUCTURE)

        // While the user is asked to authenticate, we already try to build the fill response asynchronously.
        fillResponse = lifecycleScope.async(Dispatchers.IO) { fillHandler.handle(structure, forceUnlock = true) }

        // Currently we are only using the biometric prompt API here. We need a fallback where this
        // is not available:
        // https://github.com/mozilla-mobile/android-components/issues/9713
        val executor = ContextCompat.getMainExecutor(this)
        val biometricPrompt = BiometricPrompt(this, executor, PromptCallback())

        val promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setAllowedAuthenticators(
                BiometricManager.Authenticators.BIOMETRIC_WEAK or BiometricManager.Authenticators.DEVICE_CREDENTIAL
            )
            .setTitle(
                getString(R.string.mozac_feature_autofill_popup_unlock_application, configuration.applicationName)
            )
            .build()

        biometricPrompt.authenticate(promptInfo)
    }

    internal inner class PromptCallback : BiometricPrompt.AuthenticationCallback() {
        override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
            setResult(RESULT_CANCELED)
            finish()
        }

        override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
            configuration.lock.unlock()

            val replyIntent = Intent().apply {
                // At this point it should be safe to block since the fill response should be ready once
                // the user has authenticated.
                runBlocking { putExtra(AutofillManager.EXTRA_AUTHENTICATION_RESULT, fillResponse?.await()) }
            }

            setResult(RESULT_OK, replyIntent)
            finish()
        }

        override fun onAuthenticationFailed() {
            setResult(RESULT_CANCELED)
            finish()
        }
    }
}
