/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import GeckoProvider
import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.IntentReceiverActivity

/**
 * This activity is used for performance testing with Raptor/tp6:
 * https://wiki.mozilla.org/Performance_sheriffing/Raptor
 */
class BrowserPerformanceTestActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        GeckoProvider.testConfig = SafeIntent(intent).extras

        val intent = Intent(intent)

        intent.setClassName(applicationContext, IntentReceiverActivity::class.java.name)

        startActivity(intent)

        finish()
    }
}
