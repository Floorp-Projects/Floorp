/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import com.facebook.flipper.android.AndroidFlipperClient
import com.facebook.flipper.android.utils.FlipperUtils
import com.facebook.flipper.plugins.inspector.DescriptorMapping
import com.facebook.flipper.plugins.inspector.InspectorFlipperPlugin
import com.facebook.flipper.plugins.leakcanary.LeakCanaryFlipperPlugin
import com.facebook.flipper.plugins.leakcanary.RecordLeakService
import com.facebook.flipper.plugins.sharedpreferences.SharedPreferencesFlipperPlugin
import com.facebook.soloader.SoLoader
import com.squareup.leakcanary.AndroidHeapDumper
import com.squareup.leakcanary.HeapDumper
import com.squareup.leakcanary.LeakCanary
import com.squareup.leakcanary.internal.LeakCanaryInternals
import org.mozilla.fenix.R.string.pref_key_leakcanary
import org.mozilla.fenix.ext.getPreferenceKey
import java.io.File

class DebugFenixApplication : FenixApplication() {

    override fun onCreate() {
        super.onCreate()
        SoLoader.init(this, false)

        if (FlipperUtils.shouldEnableFlipper(this)) {
            AndroidFlipperClient.getInstance(this).apply {
                addPlugin(InspectorFlipperPlugin(this@DebugFenixApplication,
                    DescriptorMapping.withDefaults()))
                addPlugin(LeakCanaryFlipperPlugin())
                addPlugin(SharedPreferencesFlipperPlugin(this@DebugFenixApplication,
                    this@DebugFenixApplication.packageName + "_preferences"))
                start()
            }
        }
    }

    private var heapDumper: ToggleableHeapDumper? = null

    override fun setupLeakCanary() {
        val leakDirectoryProvider = LeakCanaryInternals.getLeakDirectoryProvider(this)
        val defaultDumper = AndroidHeapDumper(this, leakDirectoryProvider)
        heapDumper = ToggleableHeapDumper(this, defaultDumper)
        LeakCanary.refWatcher(this)
            .listenerServiceClass(RecordLeakService::class.java)
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
