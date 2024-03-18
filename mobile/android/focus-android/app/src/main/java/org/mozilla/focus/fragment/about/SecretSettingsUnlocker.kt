/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.about

import android.content.Context
import android.widget.Toast
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.state.AppAction

/**
 * Triggers the "secret" debug menu when logoView is tapped 5 times.
 */
class SecretSettingsUnlocker(private val context: Context) {

    private var secretSettingsClicks = 0
    private var lastDebugMenuToast: Toast? = null

    /**
     * Reset the [secretSettingsClicks] counter.
     */
    fun resetCounter() {
        secretSettingsClicks = 0
    }

    fun increment() {
        // Because the user will mostly likely tap the logo in rapid succession,
        // we ensure only 1 toast is shown at any given time.
        lastDebugMenuToast?.cancel()
        secretSettingsClicks += 1
        when (secretSettingsClicks) {
            in 2 until SECRET_DEBUG_MENU_CLICKS -> {
                val clicksLeft = SECRET_DEBUG_MENU_CLICKS - secretSettingsClicks
                val toast = Toast.makeText(
                    context,
                    context.getString(R.string.about_debug_menu_toast_progress, clicksLeft),
                    Toast.LENGTH_SHORT,
                )
                toast.show()
                lastDebugMenuToast = toast
            }
            SECRET_DEBUG_MENU_CLICKS -> {
                Toast.makeText(
                    context,
                    R.string.about_debug_menu_toast_done,
                    Toast.LENGTH_LONG,
                ).show()
                context.components.appStore.dispatch(AppAction.SecretSettingsStateChange(true))
            }
        }
    }

    companion object {
        // Number of clicks on the app logo to enable the "secret" debug menu.
        private const val SECRET_DEBUG_MENU_CLICKS = 5
    }
}
