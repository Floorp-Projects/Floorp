/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.os.Build.VERSION.SDK_INT
import android.os.Build.VERSION_CODES
import android.view.View
import android.view.WindowManager
import androidx.core.view.ViewCompat
import androidx.core.view.ViewCompat.onApplyWindowInsets
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import mozilla.components.support.base.log.logger.Logger

/**
 * Attempts to enter immersive mode - fullscreen with the status bar and navigation buttons hidden,
 * expanding itself into the notch area for devices running API 28+.
 *
 * This will automatically register and use an inset listener: [View.OnApplyWindowInsetsListener]
 * to restore immersive mode if interactions with various other widgets like the keyboard or dialogs
 * got the activity out of immersive mode without [exitImmersiveMode] being called.
 */
fun Activity.enterImmersiveMode(
    insetsController: WindowInsetsControllerCompat = window.createWindowInsetsController(),
) {
    insetsController.hideInsets()

    ViewCompat.setOnApplyWindowInsetsListener(window.decorView) { view, insetsCompat ->
        if (insetsCompat.isVisible(WindowInsetsCompat.Type.statusBars())) {
            insetsController.hideInsets()
        }
        // Allow the decor view to have a chance to process the incoming WindowInsets.
        onApplyWindowInsets(view, insetsCompat)
    }

    if (SDK_INT >= VERSION_CODES.P) {
        window.setFlags(
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
        )
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
    }
}

private fun WindowInsetsControllerCompat.hideInsets() {
    apply {
        hide(WindowInsetsCompat.Type.systemBars())
        systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
}

/**
 * Shows the system UI windows that were hidden, thereby exiting the immersive experience.
 * For devices running API 28+, this function also restores the application's use
 * of the notch area of the phone to the default behavior.
 *
 * @param insetsController is an optional [WindowInsetsControllerCompat] object for controlling the
 * window insets.
 */
fun Activity.exitImmersiveMode(
    insetsController: WindowInsetsControllerCompat = window.createWindowInsetsController(),
) {
    insetsController.show(WindowInsetsCompat.Type.systemBars())

    ViewCompat.setOnApplyWindowInsetsListener(window.decorView, null)

    if (SDK_INT >= VERSION_CODES.P) {
        window.clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT
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
