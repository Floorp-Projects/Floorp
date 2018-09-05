package org.mozilla.focus.observer

import android.os.SystemClock
import android.support.v4.app.Fragment
import android.util.Log
import org.mozilla.focus.architecture.NonNullObserver
import org.mozilla.focus.session.Session
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

        session.loading.observe(fragment, object : NonNullObserver<Boolean>() {
            public override fun onValueChanged(t: Boolean) {
                if (t) {
                    if ((urlLoading != null && urlLoading != session.url.value) || urlLoading == null) {
                        urlLoading = session.url.value
                        startLoadTime = SystemClock.elapsedRealtime()
                        Log.i(LOG_TAG, "zerdatime $startLoadTime - page load start")
                    }
                } else {
                    // Progress of 99 means the page completed loading and wasn't interrupted.
                    if (urlLoading != null &&
                            session.url.value == urlLoading &&
                            session.progress.value == MAX_PROGRESS) {
                        val endTime = SystemClock.elapsedRealtime()
                        Log.i(LOG_TAG, "zerdatime $endTime - page load stop")
                        val elapsedLoad = endTime - startLoadTime
                        Log.i(LOG_TAG, "$elapsedLoad - elapsed load")
                        // Even internal pages take longer than 40 ms to load, let's not send any loads faster than this
                        if (elapsedLoad > MIN_LOAD_TIME && !UrlUtils.isLocalizedContent(urlLoading)) {
                            Log.i(LOG_TAG, "Sent load to histogram")
                            TelemetryWrapper.addLoadToHistogram(elapsedLoad)
                        }
                    }
                }
            }
        })
        session.url.observe(fragment, object : NonNullObserver<String>() {
            public override fun onValueChanged(t: String) {
                if ((urlLoading != null && urlLoading != t) || urlLoading == null) {
                    startLoadTime = SystemClock.elapsedRealtime()
                    Log.i(LOG_TAG, "zerdatime $startLoadTime - url changed to $t, new page load start")
                    urlLoading = t
                }
            }
        })
    }
}
