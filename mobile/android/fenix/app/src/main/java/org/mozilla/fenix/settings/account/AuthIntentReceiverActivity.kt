/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings

/**
 * Processes incoming intents and sends them to the corresponding activity.
 */
class AuthIntentReceiverActivity : Activity() {

    @VisibleForTesting
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        MainScope().launch {
            // The intent property is nullable, but the rest of the code below
            // assumes it is not. If it's null, then we make a new one and open
            // the HomeActivity.
            val intent = intent?.let { Intent(intent) } ?: Intent()

            if (settings().lastKnownMode.isPrivate) {
                components.intentProcessors.privateCustomTabIntentProcessor.process(intent)
            } else {
                components.intentProcessors.customTabIntentProcessor.process(intent)
            }

            intent.setClassName(applicationContext, AuthCustomTabActivity::class.java.name)
            intent.putExtra(HomeActivity.OPEN_TO_BROWSER, true)

            startActivity(intent)

            finish()
        }
    }
}
