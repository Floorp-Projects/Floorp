/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.service.glean.Glean
import mozilla.components.support.utils.Browsers
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.debug.GleanMetrics.Metrics

class GleanMetricsService(private val context: Context) : MetricsService {
    override fun start() {
        Glean.initialize(context)
        Glean.setUploadEnabled(IsGleanEnabled)

        Metrics.apply {
            defaultBrowser.set(Browsers.all(context).isDefaultBrowser)
        }
    }

    override fun track(event: Event) { }

    override fun shouldTrack(event: Event): Boolean = Settings.getInstance(context).isTelemetryEnabled

    companion object {
        private const val IsGleanEnabled = BuildConfig.TELEMETRY
    }
}
