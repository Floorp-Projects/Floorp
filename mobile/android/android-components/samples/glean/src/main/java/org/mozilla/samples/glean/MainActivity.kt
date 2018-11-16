/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_main.buttonGenerateData
import kotlinx.android.synthetic.main.activity_main.buttonSendPing
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink

open class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        Log.addSink(AndroidLogSink())

        // Initialize the Glean library. Ideally, this is the first thing that
        // must be done right after enabling logging.
        Glean.initialize(applicationContext, Configuration())

        // Set a sample value for a metric.
        GleanMetrics.Basic.os.set("Android")

        // Generate an event when user clicks on the button.
        buttonGenerateData.setOnClickListener {
            GleanMetrics.BrowserEngagement.click.record(
                    "object1",
                    "data",
                    mapOf(
                            "key1" to "extra_value_1",
                            "key2" to "extra_value_2"
                    )
            )
        }

        // Generate a "baseline" ping on click.
        buttonSendPing.setOnClickListener {
            Glean.handleEvent(Glean.PingEvent.Background)
        }
    }
}
