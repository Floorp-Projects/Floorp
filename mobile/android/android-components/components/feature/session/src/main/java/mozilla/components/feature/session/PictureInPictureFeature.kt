/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.app.Activity
import android.app.PictureInPictureParams
import android.content.pm.PackageManager
import android.os.Build
import androidx.annotation.RequiresApi
import mozilla.components.browser.session.SessionManager

/**
 * A simple implementation of Picture-in-picture mode if on a supported platform.
 *
 * @param sessionManager Session Manager for observing the selected session's fullscreen mode changes.
 * @param activity the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
 * doesn't support this.
 * @param pipChanged a change listener that allows the calling app to perform changes based on PIP mode.
 */
class PictureInPictureFeature(
    private val sessionManager: SessionManager,
    private val activity: Activity,
    private val pipChanged: ((Boolean) -> Unit?)? = null
) {
    private val hasSystemFeature = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N &&
            activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)

    fun onHomePressed(): Boolean {
        if (!hasSystemFeature) {
            return false
        }

        val fullScreenMode = sessionManager.selectedSession?.fullScreenMode ?: false
        return fullScreenMode && enterPipModeCompat()
    }

    fun enterPipModeCompat() = when {
        !hasSystemFeature -> false
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ->
            enterPipModeForO()
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.N ->
            enterPipModeForN()
        else -> false
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private fun enterPipModeForO() =
        activity.enterPictureInPictureMode(PictureInPictureParams.Builder().build())

    @Suppress("DEPRECATION")
    @RequiresApi(Build.VERSION_CODES.N)
    private fun enterPipModeForN() = run {
        activity.enterPictureInPictureMode()
        true
    }

    fun onPictureInPictureModeChanged(enabled: Boolean) = pipChanged?.invoke(enabled)
}
