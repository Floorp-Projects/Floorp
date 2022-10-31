/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import org.mozilla.samples.browser.ext.components

class IntentReceiverActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        MainScope().launch {
            val intent = intent?.let { Intent(it) } ?: Intent()
            val intentProcessors = components.externalAppIntentProcessors + components.tabIntentProcessor

            // Explicitly remove the new task and clear task flags (Our browser activity is a single
            // task activity and we never want to start a second task here).
            intent.flags = intent.flags and Intent.FLAG_ACTIVITY_NEW_TASK.inv()
            intent.flags = intent.flags and Intent.FLAG_ACTIVITY_CLEAR_TASK.inv()

            // LauncherActivity is started with the "excludeFromRecents" flag (set in manifest). We
            // do not want to propagate this flag from the launcher activity to the browser.
            intent.flags = intent.flags and Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS.inv()

            val processor = intentProcessors.firstOrNull { it.process(intent) }

            val activityClass = if (processor in components.externalAppIntentProcessors) {
                ExternalAppBrowserActivity::class
            } else {
                BrowserActivity::class
            }

            intent.setClassName(applicationContext, activityClass.java.name)

            finish()
            startActivity(intent)
        }
    }
}
