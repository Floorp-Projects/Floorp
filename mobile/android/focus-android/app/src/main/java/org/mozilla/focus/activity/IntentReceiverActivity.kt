/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import org.mozilla.focus.customtabs.CustomTabConfig
import org.mozilla.focus.utils.SafeIntent

/**
 * This activity receives VIEW intents and either forwards them to MainActivity or CustomTabActivity.
 */
class IntentReceiverActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)

        if (CustomTabConfig.isCustomTabIntent(intent)) {
            dispatchCustomTabsIntent()
        } else {
            dispatchNormalIntent()
        }

        finish()
    }

    private fun dispatchCustomTabsIntent() {
        val intent = Intent(intent)
        intent.setClassName(applicationContext, CustomTabActivity::class.java.name)

        filterFlags(intent)

        startActivity(intent)
    }

    private fun dispatchNormalIntent() {
        val intent = Intent(intent)
        intent.setClassName(applicationContext, MainActivity::class.java.name)

        filterFlags(intent)

        startActivity(intent)
    }

    private fun filterFlags(intent: Intent) {
        // Explicitly remove the new task and clear task flags (Our browser activity is a single
        // task activity and we never want to start a second task here). See bug 1280112.
        intent.flags = intent.flags and Intent.FLAG_ACTIVITY_NEW_TASK.inv()
        intent.flags = intent.flags and Intent.FLAG_ACTIVITY_CLEAR_TASK.inv()

        // LauncherActivity is started with the "exclude from recents" flag (set in manifest). We do
        // not want to propagate this flag from the launcher activity to the browser.
        intent.flags = intent.flags and Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS.inv()
    }
}
