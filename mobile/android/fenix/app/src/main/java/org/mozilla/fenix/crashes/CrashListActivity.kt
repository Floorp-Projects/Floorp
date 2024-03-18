/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.content.Intent
import android.net.Uri
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.ui.AbstractCrashListActivity
import org.mozilla.fenix.ext.components

/**
 * Activity showing the list of past crashes.
 */
class CrashListActivity : AbstractCrashListActivity() {
    override val crashReporter: CrashReporter by lazy { components.analytics.crashReporter }

    override fun onCrashServiceSelected(url: String) {
        val intent = Intent(Intent.ACTION_VIEW)
        intent.data = Uri.parse(url)
        intent.`package` = packageName
        startActivity(intent)
        finish()
    }
}
