/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.app.Activity
import android.os.Build
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import androidx.fragment.app.FragmentManager
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.prompts.dialog.FullScreenNotification
import mozilla.components.feature.prompts.dialog.FullScreenNotificationDialog
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.view.enterImmersiveMode
import mozilla.components.support.ktx.android.view.exitImmersiveMode
import org.mozilla.focus.R
import org.mozilla.focus.ext.disableDynamicBehavior
import org.mozilla.focus.ext.enableDynamicBehavior
import org.mozilla.focus.ext.hide
import org.mozilla.focus.ext.showAsFixed
import org.mozilla.focus.utils.Settings

@Suppress("LongParameterList")
class FullScreenIntegration(
    val activity: Activity,
    val store: BrowserStore,
    tabId: String?,
    sessionUseCases: SessionUseCases,
    private val settings: Settings,
    private val toolbarView: BrowserToolbar,
    private val statusBar: View,
    private val engineView: EngineView,
    private val parentFragmentManager: FragmentManager,
) : LifecycleAwareFeature, UserInteractionHandler {
    @VisibleForTesting
    internal var feature = FullScreenFeature(
        store,
        sessionUseCases,
        tabId,
        ::viewportFitChanged,
        ::fullScreenChanged,
    )

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    @VisibleForTesting
    internal fun fullScreenChanged(
        enabled: Boolean,
        fullScreenNotification: FullScreenNotification =
            FullScreenNotificationDialog(R.layout.dialog_full_screen_notification),
    ) {
        if (enabled) {
            enterBrowserFullscreen()
            statusBar.isVisible = false

            fullScreenNotification.show(parentFragmentManager)

            switchToImmersiveMode()
        } else {
            // If the video is in PiP, but is not in fullscreen anymore we should move the task containing
            // this activity to the back of the activity stack
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && activity.isInPictureInPictureMode) {
                activity.moveTaskToBack(false)
            }
            statusBar.isVisible = true
            exitBrowserFullscreen()

            exitImmersiveMode()
        }
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    @VisibleForTesting
    internal fun viewportFitChanged(viewportFit: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            activity.window.attributes.layoutInDisplayCutoutMode = viewportFit
        }
    }

    /**
     * Hide system bars. They can be revealed temporarily with system gestures, such as swiping from
     * the top of the screen. These transient system bars will overlay app’s content, may have some
     * degree of transparency, and will automatically hide after a short timeout.
     */
    @VisibleForTesting
    internal fun switchToImmersiveMode() {
        activity.enterImmersiveMode()
    }

    /**
     * Show the system bars again.
     */
    fun exitImmersiveMode() {
        activity.exitImmersiveMode()
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
