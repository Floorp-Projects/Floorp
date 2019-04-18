/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.support.utils.SafeIntent
import org.mozilla.samples.browser.ext.components

class IntentReceiverActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = intent?.let { Intent(it) } ?: Intent()

        components.sessionIntentProcessor.process(intent)

        if (CustomTabConfig.isCustomTabIntent(SafeIntent(intent))) {
            intent.setClassName(applicationContext, CustomTabActivity::class.java.name)
        } else {
            intent.setClassName(applicationContext, BrowserActivity::class.java.name)
        }

        startActivity(intent)
        finish()
    }
}
