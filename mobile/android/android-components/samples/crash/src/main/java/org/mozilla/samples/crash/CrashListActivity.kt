/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.crash

import android.widget.Toast
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.ui.AbstractCrashListActivity

/**
 * Activity showing list of past crashes.
 */
class CrashListActivity : AbstractCrashListActivity() {
    override val crashReporter: CrashReporter
        get() = (application as CrashApplication).crashReporter

    override fun onCrashServiceSelected(url: String) {
        Toast.makeText(this, "Go to: $url", Toast.LENGTH_SHORT).show()
    }
}
