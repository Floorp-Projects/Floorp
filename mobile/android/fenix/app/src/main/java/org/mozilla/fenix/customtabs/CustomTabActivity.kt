/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.os.Bundle
import androidx.navigation.fragment.NavHostFragment
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment

class CustomTabActivity : HomeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val sessionId = SafeIntent(intent).getStringExtra(IntentProcessor.ACTIVE_SESSION_ID)
        val host = supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment

        host.navController.navigate(R.id.action_global_browser, Bundle().apply {
            putString(BrowserFragment.SESSION_ID, sessionId)
        })
    }
}
