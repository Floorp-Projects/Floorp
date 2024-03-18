/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.view.View
import android.widget.Toast
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings

/**
 * Triggers the "secret" debug menu when logoView is tapped 5 times.
 */
class SecretDebugMenuTrigger(
    logoView: View,
    private val settings: Settings,
) : View.OnClickListener, DefaultLifecycleObserver {

    private var secretDebugMenuClicks = 0
    private var lastDebugMenuToast: Toast? = null

    init {
        if (!settings.showSecretDebugMenuThisSession) {
            logoView.setOnClickListener(this)
        }
    }

    /**
     * Reset the [secretDebugMenuClicks] counter.
     */
    override fun onResume(owner: LifecycleOwner) {
        secretDebugMenuClicks = 0
    }

    override fun onClick(v: View) {
        // Because the user will mostly likely tap the logo in rapid succession,
        // we ensure only 1 toast is shown at any given time.
        lastDebugMenuToast?.cancel()
        secretDebugMenuClicks += 1
        when (secretDebugMenuClicks) {
            in 2 until SECRET_DEBUG_MENU_CLICKS -> {
                val clicksLeft = SECRET_DEBUG_MENU_CLICKS - secretDebugMenuClicks
                val toast = Toast.makeText(
                    v.context,
                    v.context.getString(R.string.about_debug_menu_toast_progress, clicksLeft),
                    Toast.LENGTH_SHORT,
                )
                toast.show()
                lastDebugMenuToast = toast
            }
            SECRET_DEBUG_MENU_CLICKS -> {
                Toast.makeText(
                    v.context,
                    R.string.about_debug_menu_toast_done,
                    Toast.LENGTH_LONG,
                ).show()
                settings.showSecretDebugMenuThisSession = true
            }
        }
    }

    companion object {
        // Number of clicks on the app logo to enable the "secret" debug menu.
        private const val SECRET_DEBUG_MENU_CLICKS = 5
    }
}
