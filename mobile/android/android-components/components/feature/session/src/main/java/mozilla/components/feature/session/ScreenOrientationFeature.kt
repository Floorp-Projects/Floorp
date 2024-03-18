/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.activity.OrientationDelegate
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature that automatically reacts to [Engine] requests of updating the app's screen orientation.
 */
class ScreenOrientationFeature(
    private val engine: Engine,
    private val activity: Activity,
) : LifecycleAwareFeature, OrientationDelegate {
    override fun start() {
        engine.registerScreenOrientationDelegate(this)
    }

    override fun stop() {
        engine.unregisterScreenOrientationDelegate()
    }

    override fun onOrientationLock(requestedOrientation: Int): Boolean {
        activity.requestedOrientation = requestedOrientation
        return true
    }

    override fun onOrientationUnlock() {
        // As indicated by GeckoView - https://bugzilla.mozilla.org/show_bug.cgi?id=1744101#c3
        activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }
}
