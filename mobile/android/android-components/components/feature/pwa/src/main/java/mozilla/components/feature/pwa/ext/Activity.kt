/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifest.Orientation

/**
 * Sets the requested orientation of the [Activity] to the orientation provided by the given [WebAppManifest] (See
 * [WebAppManifest.orientation] and [WebAppManifest.Orientation].
 */
fun Activity.applyOrientation(manifest: WebAppManifest?) {
    requestedOrientation = when (manifest?.orientation) {
        Orientation.NATURAL, Orientation.ANY, null -> ActivityInfo.SCREEN_ORIENTATION_USER
        Orientation.LANDSCAPE -> ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
        Orientation.LANDSCAPE_PRIMARY -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        Orientation.LANDSCAPE_SECONDARY -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
        Orientation.PORTRAIT -> ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
        Orientation.PORTRAIT_PRIMARY -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        Orientation.PORTRAIT_SECONDARY -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
    }
}
