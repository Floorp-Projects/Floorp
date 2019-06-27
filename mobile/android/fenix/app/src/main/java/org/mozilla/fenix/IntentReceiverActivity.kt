/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.components.NotificationManager.Companion.RECEIVE_TABS_TAG
import org.mozilla.fenix.customtabs.CustomTabActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class IntentReceiverActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // The intent property is nullable, but the rest of the code below
        // assumes it is not. If it's null, then we make a new one and open
        // the HomeActivity.
        val intent = intent?.let { Intent(intent) } ?: Intent()

        val isPrivate = Settings.getInstance(this).usePrivateMode

        if (isPrivate) {
            components.utils.privateIntentProcessor.process(intent)
        } else {
            components.utils.intentProcessor.process(intent)
        }

        val openToBrowser = when {
            CustomTabConfig.isCustomTabIntent(SafeIntent(intent)) -> {
                intent.setClassName(applicationContext, CustomTabActivity::class.java.name)
                true
            }
            intent.action == Intent.ACTION_VIEW -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                if (!intent.getBooleanExtra(RECEIVE_TABS_TAG, false)) {
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

        startActivity(intent)

        finish()
    }
}
