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
import mozilla.components.browser.session.runWithSessionIdOrSelected
import mozilla.components.support.base.crash.CrashReporting
import mozilla.components.support.base.log.logger.Logger

/**
 * A simple implementation of Picture-in-picture mode if on a supported platform.
 *
 * @param sessionManager Session Manager for observing the selected session's fullscreen mode changes.
 * @param activity the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
 * doesn't support this.
 * @param crashReporting Instance of `CrashReporting` to record unexpected caught exceptions
 * @param customTabSessionId ID of custom tab session.
 * @param pipChanged a change listener that allows the calling app to perform changes based on PIP mode.
 */
class PictureInPictureFeature(
    private val sessionManager: SessionManager,
    private val activity: Activity,
    private val crashReporting: CrashReporting? = null,
    private val customTabSessionId: String? = null,
    private val pipChanged: ((Boolean) -> Unit?)? = null
) {
    internal val logger = Logger("PictureInPictureFeature")

    private val hasSystemFeature = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N &&
            activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)

    fun onHomePressed(): Boolean {
        if (!hasSystemFeature) {
            return false
        }

        val fullScreenMode =
            sessionManager.runWithSessionIdOrSelected(customTabSessionId) { session ->
                session.fullScreenMode
            }
        return fullScreenMode && try {
            enterPipModeCompat()
        } catch (e: IllegalStateException) {
            // On certain Samsung devices, if accessibility mode is enabled, this will throw an
            // IllegalStateException even if we check for the system feature beforehand. So let's
            // catch it, log it, and not enter PiP. See https://stackoverflow.com/q/55288858
            logger.warn("Entering PipMode failed", e)
            crashReporting?.submitCaughtException(e)
            false
        }
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
