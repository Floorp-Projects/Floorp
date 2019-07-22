/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.components.NotificationManager
import org.mozilla.fenix.customtabs.AuthCustomTabActivity
import org.mozilla.fenix.customtabs.CustomTabActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class IntentReceiverActivity : Activity() {

    @Suppress("ComplexMethod")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val isPrivate = Settings.getInstance(this).usePrivateMode

        MainScope().launch {
            // The intent property is nullable, but the rest of the code below
            // assumes it is not. If it's null, then we make a new one and open
            // the HomeActivity.
            val intent = intent?.let { Intent(intent) } ?: Intent()

            val intentProcessors = listOf(
                components.utils.customTabIntentProcessor,
                if (isPrivate) components.utils.privateIntentProcessor else components.utils.intentProcessor
            )

            intentProcessors.any { it.process(intent) }
            setIntentActivity(intent)

            startActivity(intent)

            finish()
        }
    }

    private fun setIntentActivity(intent: Intent) {
        val openToBrowser = when {
            components.utils.customTabIntentProcessor.matches(intent) -> {
                val activityClass = if (intent.hasExtra(getString(R.string.intent_extra_auth))) {
                    AuthCustomTabActivity::class
                } else {
                    CustomTabActivity::class
                }
                intent.setClassName(applicationContext, activityClass.java.name)
                true
            }
            intent.action == Intent.ACTION_VIEW -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                if (!intent.getBooleanExtra(NotificationManager.RECEIVE_TABS_TAG, false)) {
                    intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                }
                true
            }
            else -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                false
            }
        }

        intent.putExtra(HomeActivity.OPEN_TO_BROWSER, openToBrowser)
    }
}
