/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.fullscreen

import android.annotation.SuppressLint
import android.app.Activity
import android.content.pm.ActivityInfo
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.state.MediaState.FullscreenOrientation
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature that will auto-rotate the device to the correct orientation for the media aspect ratio.
 */
class MediaFullscreenOrientationFeature(
    private val activity: Activity,
    private val store: BrowserStore
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    @ExperimentalCoroutinesApi
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.ifChanged { it.media.aggregate.activeFullscreenOrientation }
                .collect { onChanged(it.media.aggregate.activeFullscreenOrientation) }
        }
    }

    @SuppressLint("SourceLockedOrientationActivity")
    private fun onChanged(activeFullscreenOrientation: FullscreenOrientation?) {
        when (activeFullscreenOrientation) {
            FullscreenOrientation.LANDSCAPE -> activity.requestedOrientation =
                ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
            FullscreenOrientation.PORTRAIT -> activity.requestedOrientation =
                ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
            else -> activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
