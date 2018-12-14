/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.app.Application
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink

class GleanApplication : Application() {

    override fun onCreate() {
        super.onCreate()

        // We want the log messages of all builds to go to Android logcat
        Log.addSink(AndroidLogSink())

        // Initialize the Glean library. Ideally, this is the first thing that
        // must be done right after enabling logging.
        Glean.initialize(applicationContext, Configuration(applicationId = "glean"))

        // Set a sample value for a metric.
        GleanMetrics.Basic.os.set("Android")
    }
}
