/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.os.StrictMode
import androidx.preference.PreferenceManager
import leakcanary.AppWatcher
import leakcanary.LeakCanary
import org.mozilla.fenix.ext.getPreferenceKey

class DebugFenixApplication : FenixApplication() {

    override fun setupLeakCanary() {
        val isEnabled = components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            PreferenceManager.getDefaultSharedPreferences(this)
                .getBoolean(getPreferenceKey(R.string.pref_key_leakcanary), true)
        }
        updateLeakCanaryState(isEnabled)
    }

    override fun updateLeakCanaryState(isEnabled: Boolean) {
        AppWatcher.config = AppWatcher.config.copy(enabled = isEnabled)
        LeakCanary.config = LeakCanary.config.copy(dumpHeap = isEnabled)
    }
}
