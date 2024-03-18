/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.worker

import java.util.concurrent.TimeUnit

/**
 * Indicates how often the work request should be run.
 *
 * @property repeatInterval Long indicating how often the update should happen.
 * @property repeatIntervalTimeUnit The time unit of [repeatInterval].
 */
data class Frequency(
    val repeatInterval: Long,
    val repeatIntervalTimeUnit: TimeUnit,
)
