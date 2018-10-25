/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View

open class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        GleanMetrics.Basic.os.set("Android")

        // Generate an event when user clicks on the button.
        findViewById<View>(R.id.buttonWebView).setOnClickListener {
            GleanMetrics.BrowserEngagement.click.record(
                    "object1",
                    "data",
                    mapOf(
                            "key1" to "extra_value_1",
                            "key2" to "extra_value_2"
                    )
            )
        }
    }
}
