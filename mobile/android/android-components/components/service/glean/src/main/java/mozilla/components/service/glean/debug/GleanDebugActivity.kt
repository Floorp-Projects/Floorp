/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.debug

import android.app.Activity
import android.os.Bundle
import mozilla.components.service.glean.Glean
import mozilla.components.support.base.log.logger.Logger

/**
 * Debugging activity exported by glean to allow easier debugging.
 * For example, invoking debug mode in the glean sample application
 * can be done via adb using the following command:
 *
 * adb shell am start -n org.mozilla.samples.glean/mozilla.components.service.glean.debug.GleanDebugActivity
 *
 * See the adb developer docs for more info:
 * https://developer.android.com/studio/command-line/adb#am
 */
class GleanDebugActivity : Activity() {
    private val logger = Logger("glean/GleanDebugActivity")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Enable debugging options and start the application.
        intent.extras?.let {
            val debugConfig = Glean.configuration.copy(
                logPings = intent.getBooleanExtra("logPings", Glean.configuration.logPings)
            )

            // Finally set the default configuration before starting
            // the real product's activity.
            logger.info("Setting debug config $debugConfig")
            Glean.configuration = debugConfig
        }

        val intent = packageManager.getLaunchIntentForPackage(packageName)
        startActivity(intent)

        finish()
    }
}
