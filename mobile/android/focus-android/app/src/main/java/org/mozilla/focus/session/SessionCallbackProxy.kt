/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import android.view.View

import org.mozilla.focus.web.Download
import org.mozilla.focus.web.IWebView

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.content.blocking.Tracker
import org.mozilla.focus.ext.isSearch
import org.mozilla.focus.ext.shouldRequestDesktopSite
import org.mozilla.focus.utils.AppConstants

@Suppress("TooManyFunctions")
class SessionCallbackProxy(private val session: Session, private val delegate: IWebView.Callback) : IWebView.Callback {

    var isDownload = false

    override fun onPageStarted(url: String) {
        session.loading = true

        // We are always setting the progress to 5% when a new page starts loading. Otherwise it might
        // look like the browser is doing nothing (on a slow network) until we receive a progress
        // from the WebView.
        session.progress = MINIMUM_PROGRESS

        if (!AppConstants.isGeckoBuild) {
            session.securityInfo = Session.SecurityInfo(false)
            session.url = url
        }
    }

    override fun onPageFinished(isSecure: Boolean) {
        session.progress = MAXIMUM_PROGRESS
        session.loading = false
        if (!isDownload || !AppConstants.isGeckoBuild) {
            session.securityInfo = Session.SecurityInfo(
                isSecure,
                session.securityInfo.host,
                session.securityInfo.issuer
            )
        } else {
            isDownload = false
        }
    }

    override fun onSecurityChanged(isSecure: Boolean, host: String?, organization: String?) {
        if (isSecure) {
            session.securityInfo = Session.SecurityInfo(isSecure, host ?: "", organization ?: "")
        } else {
            session.securityInfo = Session.SecurityInfo(false)
        }
    }

    override fun onProgress(progress: Int) {
        var actualProgress = progress
        // We do not want the progress to go backwards - so we always set it to at least the minimum.
        actualProgress = Math.max(MINIMUM_PROGRESS, actualProgress)

        // We do not want to show to show a progress that 100% because this will make the progress
        // bar disappear.
        actualProgress = Math.min(MAXIMUM_PROGRESS, actualProgress)

        session.progress = actualProgress
    }

    override fun onURLChanged(url: String) {
        session.url = url
    }

    override fun onTitleChanged(title: String) {
        session.title = title
    }

    override fun onRequest(isTriggeredByUserGesture: Boolean) {
        if (isTriggeredByUserGesture && session.isSearch) {
            // The user actively navigated away (no redirect) from the search page. Clear the
            // search terms.
            session.searchTerms = ""
        }
    }

    override fun countBlockedTracker() {
        // The browser-engine component will fill "trackersBlocked" with blocked URLs. Until we use
        // this component we just add a random string because we only display the count currently.
        session.trackersBlocked = session.trackersBlocked + listOf(Tracker(session.url))
    }

    override fun resetBlockedTrackers() {
        session.trackersBlocked = emptyList()
    }

    override fun onBlockingStateChanged(isBlockingEnabled: Boolean) {
        session.trackerBlockingEnabled = isBlockingEnabled
    }

    override fun onHttpAuthRequest(callback: IWebView.HttpAuthCallback, host: String, realm: String) {
        delegate.onHttpAuthRequest(callback, host, realm)
    }

    override fun onRequestDesktopStateChanged(shouldRequestDesktop: Boolean) {
        session.shouldRequestDesktopSite = shouldRequestDesktop
    }

    override fun onDownloadStart(download: Download) {
        // To be replaced with session property
        isDownload = true
        delegate.onDownloadStart(download)
    }

    override fun onLongPress(hitTarget: IWebView.HitTarget) {
        // To be replaced with session property
        delegate.onLongPress(hitTarget)
    }

    override fun onEnterFullScreen(callback: IWebView.FullscreenCallback, view: View?) {
        // To be replaced with session property
        delegate.onEnterFullScreen(callback, view)
    }

    override fun onExitFullScreen() {
        // To be replaced with session property
        delegate.onExitFullScreen()
    }

    companion object {
        internal const val MINIMUM_PROGRESS = 5
        internal const val MAXIMUM_PROGRESS = 99
    }
}
