/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import mozilla.components.support.utils.RunWhenReadyQueue
import org.mozilla.fenix.perf.ColdStartupDurationTelemetry
import org.mozilla.fenix.perf.VisualCompletenessQueue
import org.mozilla.fenix.perf.lazyMonitored

/**
 * Component group for all functionality related to performance.
 */
class PerformanceComponent {
    val visualCompletenessQueue by lazyMonitored { VisualCompletenessQueue(RunWhenReadyQueue()) }
    val coldStartupDurationTelemetry by lazyMonitored { ColdStartupDurationTelemetry() }
}
