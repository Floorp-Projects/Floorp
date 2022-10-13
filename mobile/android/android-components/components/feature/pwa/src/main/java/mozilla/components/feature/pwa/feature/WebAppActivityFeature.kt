/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.app.Activity
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.extension.toIconRequest
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ext.applyOrientation
import mozilla.components.feature.pwa.ext.toTaskDescription
import mozilla.components.support.ktx.android.view.enterToImmersiveMode

/**
 * Feature used to handle window effects for "standalone" and "fullscreen" web apps.
 */
class WebAppActivityFeature(
    private val activity: Activity,
    private val icons: BrowserIcons,
    private val manifest: WebAppManifest,
) : DefaultLifecycleObserver {

    private val scope = MainScope()

    override fun onResume(owner: LifecycleOwner) {
        if (manifest.display == WebAppManifest.DisplayMode.FULLSCREEN) {
            activity.enterToImmersiveMode()
        }

        activity.applyOrientation(manifest)

        scope.launch {
            updateRecentEntry()
        }
    }

    override fun onDestroy(owner: LifecycleOwner) {
        scope.cancel()
    }

    @VisibleForTesting
    internal suspend fun updateRecentEntry() {
        val icon = icons.loadIcon(manifest.toIconRequest()).await()

        activity.setTaskDescription(manifest.toTaskDescription(icon.bitmap))
    }
}
