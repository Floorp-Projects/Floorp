/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.request

import android.content.Intent

class LaunchIntentMetadata(
    val url: String,
    val appIntent: Intent?
) {
    companion object {
        val blank = LaunchIntentMetadata("about:blank", null)
    }
}
