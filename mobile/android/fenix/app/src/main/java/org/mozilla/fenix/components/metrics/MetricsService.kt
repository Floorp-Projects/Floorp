/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

enum class MetricServiceType {
    Data, Marketing
}

interface MetricsService {
    val type: MetricServiceType

    fun start()
    fun stop()
    fun track(event: Event)
    fun shouldTrack(event: Event): Boolean
}
