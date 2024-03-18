/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import android.content.Intent

/**
 * State keeping track of app intents to launch.
 *
 * @param url the URL to launch in an external app.
 * @param appIntent the [Intent] to launch.
 */
data class AppIntentState(
    val url: String,
    val appIntent: Intent?,
)
