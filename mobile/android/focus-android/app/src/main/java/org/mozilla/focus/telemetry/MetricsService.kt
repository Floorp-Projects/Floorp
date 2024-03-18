/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Context

/**
 * A service for tracking telemetry events in various parts of the app.
 */
interface MetricsService {

    /**
     * Perform any initialization that the service may require.
     */
    fun initialize(context: Context)
}
