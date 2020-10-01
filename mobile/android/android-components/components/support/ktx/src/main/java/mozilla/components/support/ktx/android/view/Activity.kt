/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Deprecation to be fixed in https://github.com/mozilla-mobile/android-components/issues/8517
@file:Suppress("DEPRECATION")

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.os.Build
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import mozilla.components.support.base.log.logger.Logger

/**
 * Attempts to call immersive mode using the View to hide the status bar and navigation buttons.
 */
@Suppress("DEPRECATION")
fun Activity.enterToImmersiveMode() {
    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        window.insetsController?.apply {
            systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsets.Type.systemBars())
        }
    } else {
        window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_FULLSCREEN
            or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
    }
}

/**
 * Attempts to come out from immersive mode using the View.
 */
@Suppress("DEPRECATION")
fun Activity.exitImmersiveModeIfNeeded() {
    if (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON and window.attributes.flags == 0) {
        // We left immersive mode already.
        return
    }

    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        window.insetsController?.show(WindowInsets.Type.systemBars())
    } else {
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
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
