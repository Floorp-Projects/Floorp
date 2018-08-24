/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import android.os.Environment
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.support.base.observer.Consumable

internal class EngineObserver(val session: Session) : EngineSession.Observer {

    override fun onLocationChange(url: String) {
        session.url = url
        session.searchTerms = ""
        session.title = ""
    }

    override fun onTitleChange(title: String) {
        session.title = title
    }

    override fun onProgress(progress: Int) {
        session.progress = progress
    }

    override fun onLoadingStateChange(loading: Boolean) {
        session.loading = loading
        if (loading) {
            session.trackersBlocked = emptyList()
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let { session.canGoBack = canGoBack }
        canGoForward?.let { session.canGoForward = canGoForward }
    }

    override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
        session.securityInfo = Session.SecurityInfo(secure, host
                ?: "", issuer ?: "")
    }

    override fun onTrackerBlocked(url: String) {
        session.trackersBlocked += url
    }

    override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
        session.trackerBlockingEnabled = enabled
    }

    override fun onLongPress(hitResult: HitResult) {
        session.hitResult = Consumable.from(hitResult)
    }

    override fun onExternalResource(
        url: String,
        fileName: String?,
        contentLength: Long?,
        contentType: String?,
        cookie: String?,
        userAgent: String?
    ) {
        val download = Download(url, fileName, contentType, contentLength, userAgent, Environment.DIRECTORY_DOWNLOADS)
        session.download = Consumable.from(download)
    }
}
