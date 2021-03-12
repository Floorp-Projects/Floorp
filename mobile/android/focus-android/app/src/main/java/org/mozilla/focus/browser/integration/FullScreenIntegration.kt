/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.app.Activity
import android.os.Build
import android.view.View
import android.view.WindowManager
import androidx.annotation.RequiresApi
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.focus.browser.DisplayToolbar

class FullScreenIntegration(
    val activity: Activity,
    val store: BrowserStore,
    val sessionManager: SessionManager,
    tabId: String?,
    private val toolbarView: DisplayToolbar,
    private val statusBar: View
) : LifecycleAwareFeature, UserInteractionHandler {
    private val feature = FullScreenFeature(
        store,
        SessionUseCases(store, sessionManager),
        tabId,
        ::viewportFitChanged,
        ::fullScreenChanged
    )

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    private fun fullScreenChanged(enabled: Boolean) {
        if (enabled) {
            toolbarView.setExpanded(false, true)
            statusBar.visibility = View.GONE

            switchToImmersiveMode()
        } else {
            toolbarView.setExpanded(true, true)
            statusBar.visibility = View.VISIBLE

            exitImmersiveModeIfNeeded()
        }
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    @RequiresApi(Build.VERSION_CODES.P)
    private fun viewportFitChanged(viewportFit: Int) {
        activity.window.attributes.layoutInDisplayCutoutMode = viewportFit
    }

    /**
     * Hide system bars. They can be revealed temporarily with system gestures, such as swiping from
     * the top of the screen. These transient system bars will overlay app’s content, may have some
     * degree of transparency, and will automatically hide after a short timeout.
     */
    private fun switchToImmersiveMode() {
        val window = activity.window
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
    }

    /**
     * Show the system bars again.
     */
    fun exitImmersiveModeIfNeeded() {
        if (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON and activity.window.attributes.flags == 0) {
            // We left immersive mode already.
            return
        }

        val window = activity.window
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
    }
}
