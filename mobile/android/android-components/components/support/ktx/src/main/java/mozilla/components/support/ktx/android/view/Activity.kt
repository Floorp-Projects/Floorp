/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.os.Build
import android.view.View
import android.view.WindowInsets.Type.statusBars
import android.view.WindowManager
import androidx.annotation.VisibleForTesting
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import mozilla.components.support.base.log.logger.Logger

/**
 * Attempts to enter immersive mode - fullscreen with the status bar and navigation buttons hidden.
 * This will automatically register and use an
 * - an inset listener: [View.OnApplyWindowInsetsListener] on API 30+ or
 * - a system visibility listener: [View.OnSystemUiVisibilityChangeListener] for below APIs.
 *
 * to restore immersive mode if interactions with various other widgets like the keyboard or dialogs
 * got the activity out of immersive mode without [exitImmersiveModeIfNeeded] being called.
 */
fun Activity.enterToImmersiveMode() {
    setAsImmersive()
    enableImmersiveModeRestore()
}

@VisibleForTesting
internal fun Activity.setAsImmersive() {
    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    window.getWindowInsetsController().apply {
        hide(WindowInsetsCompat.Type.systemBars())
        systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
}

/**
 * Anytime a new Window is focused like of a Dialog or of the keyboard the activity
 * will exit immersive mode.
 * This will observe such events and set again the activity to be immersive
 * the next time it gets focused.
 */
@VisibleForTesting
internal fun Activity.enableImmersiveModeRestore() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        window.decorView.setOnApplyWindowInsetsListener { view, insets ->
            if (insets.isVisible(statusBars())) {
                setAsImmersive()
            }
            // Allow the decor view to have a chance to process the incoming WindowInsets.
            view.onApplyWindowInsets(insets)
        }
    } else {
        @Suppress("DEPRECATION") // insets.isVisible(int) is available only starting with API 30
        window.decorView.setOnSystemUiVisibilityChangeListener { newFlags ->
            if (newFlags and View.SYSTEM_UI_FLAG_FULLSCREEN == 0) {
                setAsImmersive()
            }
        }
    }
}

/**
 * Attempts to come out from immersive mode.
 */
fun Activity.exitImmersiveModeIfNeeded() {
    if (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON and window.attributes.flags == 0) {
        // We left immersive mode already.
        return
    }

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        window.decorView.setOnApplyWindowInsetsListener(null)
    } else {
        @Suppress("DEPRECATION")
        window.decorView.setOnSystemUiVisibilityChangeListener(null)
    }

    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    window.getWindowInsetsController().apply {
        show(WindowInsetsCompat.Type.systemBars())
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
