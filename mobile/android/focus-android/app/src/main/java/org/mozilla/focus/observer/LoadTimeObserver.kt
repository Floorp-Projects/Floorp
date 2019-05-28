/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.observer

import android.os.SystemClock
import androidx.fragment.app.Fragment
import android.util.Log
import mozilla.components.browser.session.Session
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.UrlUtils

object LoadTimeObserver {
    private const val MIN_LOAD_TIME: Long = 40
    private const val MAX_PROGRESS = 99
    private const val LOG_TAG: String = "LoadTimeObserver"

    @JvmStatic
    fun addObservers(session: Session, fragment: Fragment) {
        var startLoadTime: Long = 0
        var urlLoading: String? = null

        session.register(object : Session.Observer {
            override fun onUrlChanged(session: Session, url: String) {
                if ((urlLoading != null && urlLoading != url) || urlLoading == null) {
                    startLoadTime = SystemClock.elapsedRealtime()
                    Log.i(LOG_TAG, "zerdatime $startLoadTime - url changed to $url, new page load start")
                    urlLoading = url
                }
            }

            override fun onLoadingStateChanged(session: Session, loading: Boolean) {
                if (loading) {
                    if ((urlLoading != null && urlLoading != session.url) || urlLoading == null) {
                        urlLoading = session.url
                        startLoadTime = SystemClock.elapsedRealtime()
                        Log.i(LOG_TAG, "zerdatime $startLoadTime - page load start")
                    }
                } else {
                    // Progress of 99 means the page completed loading and wasn't interrupted.
                    if (urlLoading != null &&
                        session.url == urlLoading &&
                        session.progress == MAX_PROGRESS) {
                        Log.i(LOG_TAG, "Loaded page at $session.url.value")
                        val endTime = SystemClock.elapsedRealtime()
                        Log.i(LOG_TAG, "zerdatime $endTime - page load stop")
                        val elapsedLoad = endTime - startLoadTime
                        Log.i(LOG_TAG, "$elapsedLoad - elapsed load")
                        // Even internal pages take longer than 40 ms to load, let's not send any loads faster than this
                        if (elapsedLoad > MIN_LOAD_TIME && !UrlUtils.isLocalizedContent(urlLoading)) {
                            Log.i(LOG_TAG, "Sent load to histogram")
                            TelemetryWrapper.addLoadToHistogram(session.url, elapsedLoad)
                        }
                    }
                }
            }
        }, owner = fragment)
    }
}
