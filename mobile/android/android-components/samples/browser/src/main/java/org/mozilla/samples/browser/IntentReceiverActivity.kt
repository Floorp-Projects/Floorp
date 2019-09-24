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

            intentProcessors.any { it.process(intent) }

            setBrowserActivity(intent)

            finish()
            startActivity(intent)
        }
    }

    /**
     * Sets the activity that this [intent] will launch.
     */
    private fun setBrowserActivity(intent: Intent) {
        val className = if (components.externalAppIntentProcessors.any { it.matches(intent) }) {
            ExternalAppBrowserActivity::class
        } else {
            BrowserActivity::class
        }

        intent.setClassName(applicationContext, className.java.name)
    }
}
