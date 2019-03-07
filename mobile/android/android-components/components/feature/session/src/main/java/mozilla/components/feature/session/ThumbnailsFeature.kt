/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.isOSOnLowMemory

/**
 * Feature implementation for automatically taking thumbnails of sites.
 * The feature will take a screenshot when the page finishes loading,
 * and will add it to the [Session.thumbnail] property.
 *
 * If the OS is under low memory conditions, the screenshot will be not taken.
 * Ideally, this should be used in conjunction with [SessionManager.onLowMemory] to allow
 * free up some [Session.thumbnail] from memory.
 */
class ThumbnailsFeature(
    private val context: Context,
    private val engineView: EngineView,
    sessionManager: SessionManager
) : LifecycleAwareFeature {

    private val observer = ThumbnailsFeatureRequestObserver(sessionManager)

    /**
     * Starts observing the selected session to listen for when a session finish loading.
     */
    override fun start() {
        observer.observeSelected()
    }

    /**
     * Stops observing the selected session.
     */
    override fun stop() {
        observer.stop()
    }

    internal inner class ThumbnailsFeatureRequestObserver(
        sessionManager: SessionManager
    ) : SelectionAwareSessionObserver(sessionManager) {

        override fun onLoadingStateChanged(session: Session, loading: Boolean) {
            if (!loading) {
                requestScreenshot(session)
            }
        }
    }

    private fun requestScreenshot(session: Session) {
        if (!isLowOnMemory()) {
            engineView.captureThumbnail {
                session.thumbnail = it
            }
        } else {
            session.thumbnail = null
        }
    }

    @VisibleForTesting
    internal var testLowMemory = false

    private fun isLowOnMemory() = testLowMemory || context.isOSOnLowMemory()
}
