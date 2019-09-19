/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import com.squareup.leakcanary.AndroidHeapDumper
import com.squareup.leakcanary.HeapDumper
import com.squareup.leakcanary.LeakCanary
import com.squareup.leakcanary.internal.LeakCanaryInternals
import org.mozilla.fenix.R.string.pref_key_leakcanary
import org.mozilla.fenix.ext.getPreferenceKey
import java.io.File

class DebugFenixApplication : FenixApplication() {

    private var heapDumper: ToggleableHeapDumper? = null

    override fun setupLeakCanary() {
        val leakDirectoryProvider = LeakCanaryInternals.getLeakDirectoryProvider(this)
        val defaultDumper = AndroidHeapDumper(this, leakDirectoryProvider)
        heapDumper = ToggleableHeapDumper(this, defaultDumper)
        LeakCanary.refWatcher(this)
            .heapDumper(heapDumper)
            .buildAndInstall()
    }

    override fun toggleLeakCanary(newValue: Boolean) {
        heapDumper?.enabled = newValue
    }

    internal class ToggleableHeapDumper(
        context: Context,
        private val defaultDumper: HeapDumper
    ) : HeapDumper {
        var prefs: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
        var enabled = prefs.getBoolean(context.getPreferenceKey(pref_key_leakcanary), false)
        override fun dumpHeap(): File? = if (enabled) defaultDumper.dumpHeap() else HeapDumper.RETRY_LATER
    }
}
