/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.app.Activity
import android.os.Build.VERSION
import android.os.Build.VERSION_CODES
import android.view.View
import android.view.Window
import android.view.WindowInsets
import android.view.WindowInsets.Type
import android.view.WindowManager
import mozilla.components.concept.base.crash.Breadcrumb

/**
 * Attempts to call immersive mode using the View to hide the status bar and navigation buttons.
 *
 * We don't use the equivalent function from Android Components because the stable flag messes
 * with the toolbar. See #1998 and #3272.
 */
@Deprecated(
    message = "Use the Android Component implementation instead.",
    replaceWith = ReplaceWith(
        "enterToImmersiveMode()",
        "mozilla.components.support.ktx.android.view.enterToImmersiveMode"
    )
)
fun Activity.enterToImmersiveMode() {
    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    // This will be addressed on https://github.com/mozilla-mobile/fenix/issues/17804
    @Suppress("DEPRECATION")
    window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_FULLSCREEN
            or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
}

fun Activity.breadcrumb(
    message: String,
    data: Map<String, String> = emptyMap()
) {
    components.analytics.crashReporter.recordCrashBreadcrumb(
        Breadcrumb(
            category = this::class.java.simpleName,
            message = message,
            data = data + mapOf(
                "instance" to this.hashCode().toString()
            ),
            level = Breadcrumb.Level.INFO
        )
    )
}

/**
 * Handles inset changes for the whole activity.
 *
 * The deprecation of [WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE] in [VERSION_CODES.R]
 * means inset changes have to be handled with a [View.OnApplyWindowInsetsListener].
 * [Window.setDecorFitsSystemWindows] false tells the system that the app will handle all insets.
 * When a keyboard is opened [WindowInsets.getInsets] of [Type.ime] updates accordingly.
 *
 * See https://github.com/mozilla-mobile/fenix/issues/17805.
 * */
fun Activity.enableSystemInsetsHandling() {
    if (VERSION.SDK_INT >= VERSION_CODES.R) {
        val currentInsetTypes = mutableSetOf<Int>()

        currentInsetTypes.add(Type.systemBars())
        currentInsetTypes.add(Type.statusBars())
        currentInsetTypes.add(Type.mandatorySystemGestures())
        currentInsetTypes.add(Type.ime())

        window.setDecorFitsSystemWindows(false)

        window.decorView.setOnApplyWindowInsetsListener { v, _ ->
            val currentInsetTypeMask = currentInsetTypes.fold(0) { accumulator, type ->
                accumulator or type
            }
            val insets = window.decorView.rootWindowInsets.getInsets(currentInsetTypeMask)
            v.setPadding(insets.left, insets.top, insets.right, insets.bottom)

            WindowInsets.Builder()
                .setInsets(currentInsetTypeMask, insets)
                .build()
        }
    } else {
        @Suppress("DEPRECATION")
        window.setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
        )
    }
}
