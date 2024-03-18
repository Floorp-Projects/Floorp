/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.app.Activity
import android.app.PictureInPictureParams
import android.content.pm.PackageManager
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.annotation.RequiresApi
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.base.log.logger.Logger

/**
 * A simple implementation of Picture-in-picture mode if on a supported platform.
 *
 * @param store Browser Store for observing the selected session's fullscreen mode changes.
 * @param activity the activity with the EngineView for calling PIP mode when required; the AndroidX Fragment
 * doesn't support this.
 * @param crashReporting Instance of `CrashReporting` to record unexpected caught exceptions
 * @param tabId ID of tab or custom tab session.
 */
class PictureInPictureFeature(
    private val store: BrowserStore,
    private val activity: Activity,
    private val crashReporting: CrashReporting? = null,
    private val tabId: String? = null,
) {
    internal val logger = Logger("PictureInPictureFeature")

    private val hasSystemFeature = SDK_INT >= Build.VERSION_CODES.N &&
        activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)

    fun onHomePressed(): Boolean {
        if (!hasSystemFeature) {
            return false
        }

        val session = store.state.findTabOrCustomTabOrSelectedTab(tabId)
        val fullScreenMode = session?.content?.fullScreen == true
        val contentIsPlaying = session?.mediaSessionState?.playbackState == MediaSession.PlaybackState.PLAYING
        return fullScreenMode && contentIsPlaying && try {
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

    /**
     * Enter Picture-in-Picture mode.
     */
    fun enterPipModeCompat() = when {
        !hasSystemFeature -> false
        SDK_INT >= Build.VERSION_CODES.O -> enterPipModeForO()
        SDK_INT >= Build.VERSION_CODES.N -> enterPipModeForN()
        else -> false
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private fun enterPipModeForO() =
        activity.enterPictureInPictureMode(PictureInPictureParams.Builder().build())

    @Suppress("Deprecation")
    @RequiresApi(Build.VERSION_CODES.N)
    private fun enterPipModeForN() = run {
        activity.enterPictureInPictureMode()
        true
    }

    /**
     * Should be called when the system informs you of changes to and from picture-in-picture mode.
     * @param enabled True if the activity is in picture-in-picture mode.
     */
    fun onPictureInPictureModeChanged(enabled: Boolean) {
        val sessionId = tabId ?: store.state.selectedTabId ?: return
        store.dispatch(ContentAction.PictureInPictureChangedAction(sessionId, enabled))
    }
}
