/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.view.ViewTreeObserver
import android.view.WindowManager
import androidx.annotation.VisibleForTesting
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import mozilla.components.support.base.log.logger.Logger

/**
 * Attempts to call immersive mode using the View to hide the status bar and navigation buttons.
 * @param onWindowFocusChangeListener optional callback to ensure immersive mode is stable
 * Note that the callback reference should be kept by the caller and be used for [exitImmersiveModeIfNeeded] call.
 */
fun Activity.enterToImmersiveMode(
    onWindowFocusChangeListener: ViewTreeObserver.OnWindowFocusChangeListener? = null
) {
    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    window.getWindowInsetsController().apply {
        hide(WindowInsetsCompat.Type.systemBars())
        systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    // We need to make sure system bars do not become permanently visible after interactions with content
    // see https://github.com/mozilla-mobile/fenix/issues/20240
    onWindowFocusChangeListener?.let {
        window.decorView.viewTreeObserver?.addOnWindowFocusChangeListener(it)
    }
}

/**
 * Attempts to come out from immersive mode using the View.
 * @param onWindowFocusChangeListener optional callback to ensure immersive mode is stable
 * Note that the callback reference should be kept by the caller and be the same used for [enterToImmersiveMode] call.
 */
@Suppress("DEPRECATION")
fun Activity.exitImmersiveModeIfNeeded(
    onWindowFocusChangeListener: ViewTreeObserver.OnWindowFocusChangeListener? = null
) {
    if (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON and window.attributes.flags == 0) {
        // We left immersive mode already.
        return
    }
    onWindowFocusChangeListener?.let {
        window.decorView.viewTreeObserver?.removeOnWindowFocusChangeListener(it)
    }
    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    window.getWindowInsetsController().apply {
        show(WindowInsetsCompat.Type.systemBars())
    }
}

/**
 * OnWindowFocusChangeListener used to ensure immersive mode is not broken by other views interactions.
 */
@VisibleForTesting
val Activity.onWindowFocusChangeListener: ViewTreeObserver.OnWindowFocusChangeListener
    get() = ViewTreeObserver.OnWindowFocusChangeListener { hasFocus ->
        if (hasFocus) {
            window.getWindowInsetsController().apply {
                hide(WindowInsetsCompat.Type.systemBars())
            }
        }
    }

/**
 * Calls [Activity.reportFullyDrawn] while also preventing crashes under some circumstances.
 *
 * @param errorLogger the logger to be used if errors are logged.
 */
fun Activity.reportFullyDrawnSafe(errorLogger: Logger) {
    try {
        reportFullyDrawn()
    } catch (e: SecurityException) {
        // This exception is throw on some Samsung devices. We were unable to identify the root
        // cause but suspect it's related to Samsung security features. See
        // https://github.com/mozilla-mobile/fenix/issues/12345#issuecomment-655058864 for details.
        //
        // We include "Fully drawn" in the log statement so that this error appears when grepping
        // for fully drawn time.
        errorLogger.error("Fully drawn - unable to call reportFullyDrawn", e)
    }
}
