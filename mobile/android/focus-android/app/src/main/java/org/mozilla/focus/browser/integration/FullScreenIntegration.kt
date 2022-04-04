/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.app.Activity
import android.os.Build
import android.view.View
import android.view.WindowManager
import androidx.annotation.RequiresApi
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.focus.ext.disableDynamicBehavior
import org.mozilla.focus.ext.enableDynamicBehavior
import org.mozilla.focus.ext.hide
import org.mozilla.focus.ext.showAsFixed
import org.mozilla.focus.utils.Settings

class FullScreenIntegration(
    val activity: Activity,
    val store: BrowserStore,
    tabId: String?,
    sessionUseCases: SessionUseCases,
    private val settings: Settings,
    private val toolbarView: BrowserToolbar,
    private val statusBar: View,
    private val engineView: EngineView,
) : LifecycleAwareFeature, UserInteractionHandler {
    @VisibleForTesting
    internal var feature = FullScreenFeature(
        store,
        sessionUseCases,
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

    @VisibleForTesting
    internal fun fullScreenChanged(enabled: Boolean) {
        if (enabled) {
            enterBrowserFullscreen()
            statusBar.visibility = View.GONE

            switchToImmersiveMode()
        } else {
            // If the video is in PiP, but is not in fullscreen anymore we should move the task containing
            // this activity to the back of the activity stack
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && activity.isInPictureInPictureMode) {
                activity.moveTaskToBack(false)
            }
            statusBar.visibility = View.VISIBLE
            exitBrowserFullscreen()

            exitImmersiveModeIfNeeded()
        }
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.P)
    internal fun viewportFitChanged(viewportFit: Int) {
        activity.window.attributes.layoutInDisplayCutoutMode = viewportFit
    }

    /**
     * Hide system bars. They can be revealed temporarily with system gestures, such as swiping from
     * the top of the screen. These transient system bars will overlay app’s content, may have some
     * degree of transparency, and will automatically hide after a short timeout.
     */
    @VisibleForTesting
    internal fun switchToImmersiveMode() {
        val window = activity.window
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/5016
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            )
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
        @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/5016
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
    }

    @VisibleForTesting
    internal fun enterBrowserFullscreen() {
        if (settings.isAccessibilityEnabled()) {
            toolbarView.hide(engineView)
        } else {
            toolbarView.collapse()
            toolbarView.disableDynamicBehavior(engineView)
        }
    }

    @VisibleForTesting
    internal fun exitBrowserFullscreen() {
        if (settings.isAccessibilityEnabled()) {
            toolbarView.showAsFixed(activity, engineView)
        } else {
            toolbarView.enableDynamicBehavior(activity, engineView)
            toolbarView.expand()
        }
    }
}
