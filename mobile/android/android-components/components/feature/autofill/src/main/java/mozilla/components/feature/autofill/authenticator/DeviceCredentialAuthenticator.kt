/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.authenticator

import android.app.Activity.RESULT_OK
import android.app.KeyguardManager
import android.content.Context
import androidx.fragment.app.FragmentActivity
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R

/**
 * [Authenticator] implementation that uses Android's [KeyguardManager] to authenticate the user.
 */
internal class DeviceCredentialAuthenticator(
    private val configuration: AutofillConfiguration,
) : Authenticator {
    private var callback: Authenticator.Callback? = null

    @Suppress("Deprecation") // This is only used when BiometricPrompt is unavailable
    override fun prompt(activity: FragmentActivity, callback: Authenticator.Callback) {
        this.callback = callback

        val manager = activity.getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager?
        val intent = manager!!.createConfirmDeviceCredentialIntent(
            activity.getString(
                R.string.mozac_feature_autofill_popup_unlock_application,
                configuration.applicationName,
            ),
            "",
        )
        activity.startActivityForResult(intent, configuration.activityRequestCode)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int) {
        if (requestCode == configuration.activityRequestCode && resultCode == RESULT_OK) {
            callback?.onAuthenticationSucceeded()
        } else {
            callback?.onAuthenticationFailed()
        }
    }

    companion object {
        fun isAvailable(context: Context): Boolean {
            val manager = context.getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager?
            return manager?.isKeyguardSecure == true
        }
    }
}
