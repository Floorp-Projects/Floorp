/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R

/**
 * Activity for displaying the list of reported crashes.
 */
abstract class AbstractCrashListActivity : AppCompatActivity() {
    abstract val crashReporter: CrashReporter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setTitle(R.string.mozac_lib_crash_activity_title)

        if (savedInstanceState == null) {
            supportFragmentManager.beginTransaction()
                .add(android.R.id.content, CrashListFragment())
                .commit()
        }
    }

    /**
     * Gets invoked whenever the user selects a crash reporting service.
     *
     * @param url URL pointing to the crash report for the selected crash reporting service.
     */
    abstract fun onCrashServiceSelected(url: String)
}
