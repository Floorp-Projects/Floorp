/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.concept.fetch.Client
import java.util.concurrent.TimeUnit

internal const val DEFAULT_REFRESH_INTERVAL = 4L
@Suppress("TopLevelPropertyNaming")
internal val DEFAULT_REFRESH_TIMEUNIT = TimeUnit.HOURS

/**
 * Indicating all details for how the pocket stories should be refreshed.
 *
 * @param client [Client] implementation used for downloading the Pocket stories.
 * @param frequency Optional - The interval at which to try and refresh items. Defaults to 1 hour.
 */
class PocketStoriesConfig(
    val client: Client,
    val frequency: Frequency = Frequency(
        DEFAULT_REFRESH_INTERVAL,
        DEFAULT_REFRESH_TIMEUNIT
    )
)

/**
 * Indicates how often the pocket stories should be refreshed.
 *
 * @param repeatInterval Integer indicating how often the update should happen.
 * @param repeatIntervalTimeUnit The time unit of the [repeatInterval].
 */
class Frequency(val repeatInterval: Long, val repeatIntervalTimeUnit: TimeUnit)
