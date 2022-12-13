/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import androidx.preference.PreferenceManager
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import leakcanary.AppWatcher
import leakcanary.LeakCanary
import org.mozilla.focus.ext.application

class DebugFocusApplication : FocusApplication() {

    @OptIn(DelicateCoroutinesApi::class)
    override fun setupLeakCanary() {
        if (!AppWatcher.isInstalled) {
            AppWatcher.manualInstall(
                application = application,
                watchersToInstall = AppWatcher.appDefaultWatchers(application),
            )
        }
        GlobalScope.launch(Dispatchers.IO) {
            val isEnabled = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getBoolean(getString(R.string.pref_key_leakcanary), true)
            updateLeakCanaryState(isEnabled)
        }
    }

    @OptIn(DelicateCoroutinesApi::class)
    override fun updateLeakCanaryState(isEnabled: Boolean) {
        GlobalScope.launch(Dispatchers.IO) {
            LeakCanary.showLeakDisplayActivityLauncherIcon(isEnabled)
            LeakCanary.config = LeakCanary.config.copy(dumpHeap = isEnabled)
        }
    }
}
