/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.Session
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry

class TelemetrySessionObserver(
    private val metrics: MetricController,
    private val ads: AdsTelemetry
) : Session.Observer {
    private var urlLoading: String? = null
    @VisibleForTesting
    var redirectChain = mutableListOf<String>()
    @VisibleForTesting
    var originSessionUrl: String? = null

    private val temporaryFix = TemporaryFix()

    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        if (loading) {
            urlLoading = session.url
        } else if (urlLoading != null && !session.private && temporaryFix.shouldSendEvent(session.url)) {
            temporaryFix.eventSentFor = session.url
            metrics.track(Event.UriOpened)
        }
    }

    /**
     * When a link is clicked, record its redirect chain as well as origin url
     */
    override fun onLoadRequest(
        session: Session,
        url: String,
        triggeredByRedirect: Boolean,
        triggeredByWebContent: Boolean
    ) {
        if (isFirstLinkInRedirectChain(url, session.url)) {
            originSessionUrl = session.url
        }
        if (canStartChain()) {
            redirectChain.add(url)
        }
    }

    private fun canStartChain(): Boolean {
        return originSessionUrl != null
    }

    private fun isFirstLinkInRedirectChain(url: String, sessionUrl: String): Boolean {
        return originSessionUrl == null && url != sessionUrl
    }

    /**
     * After the redirect chain has finished, check if we encountered an ad on the way and clear
     * the stored info for that chain
     */
    override fun onUrlChanged(session: Session, url: String) {
        ads.trackAdClickedMetric(originSessionUrl, redirectChain)
        originSessionUrl = null
        redirectChain.clear()
    }

    /**
     * Currently, [Session.Observer.onLoadingStateChanged] is called multiple times the first
     * time a new session loads a page. This is inflating our telemetry numbers, so we need to
     * handle it, but we will be able to remove this code when [onLoadingStateChanged] has
     * been fixed.
     *
     * See Fenix #3676
     * See AC https://github.com/mozilla-mobile/android-components/issues/4795
     * TODO remove this class after AC #4795 has been fixed
     */
    private class TemporaryFix {
        var eventSentFor: String? = null

        fun shouldSendEvent(newUrl: String): Boolean = eventSentFor != newUrl
    }
}
