/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.browser.session.manifest.WebAppManifest

/**
 * Sets the requested orientation of the [Activity] to the orientation provided by the given [WebAppManifest] (See
 * [WebAppManifest.orientation] and [WebAppManifest.Orientation].
 */
@Suppress("ComplexMethod") // Lots of branches... but not complex.
fun Activity.applyOrientation(manifest: WebAppManifest) {
    when (manifest.orientation) {
        WebAppManifest.Orientation.NATURAL ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER

        WebAppManifest.Orientation.LANDSCAPE ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE

        WebAppManifest.Orientation.LANDSCAPE_PRIMARY ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE

        WebAppManifest.Orientation.LANDSCAPE_SECONDARY ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE

        WebAppManifest.Orientation.PORTRAIT ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT

        WebAppManifest.Orientation.PORTRAIT_PRIMARY ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT

        WebAppManifest.Orientation.PORTRAIT_SECONDARY ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT

        WebAppManifest.Orientation.ANY ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER

        else ->
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
    }
}
