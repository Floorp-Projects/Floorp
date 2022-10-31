/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import java.util.concurrent.TimeUnit

/**
 * Get a list of all story impressions (expressed in seconds from Epoch) in the period between
 * `now` down to [PocketSponsoredStoryCaps.flightPeriod].
 */
fun PocketSponsoredStory.getCurrentFlightImpressions(): List<Long> {
    val now = TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis())
    return caps.currentImpressions.filter {
        now - it < caps.flightPeriod
    }
}

/**
 * Get if this story was already shown for the maximum number of times available in it's lifetime.
 */
fun PocketSponsoredStory.hasLifetimeImpressionsLimitReached(): Boolean {
    return caps.currentImpressions.size >= caps.lifetimeCount
}

/**
 * Get if this story was already shown for the maximum number of times available in the period
 * specified by [PocketSponsoredStoryCaps.flightPeriod].
 */
fun PocketSponsoredStory.hasFlightImpressionsLimitReached(): Boolean {
    return getCurrentFlightImpressions().size >= caps.flightCount
}

/**
 * Record a new impression at this instant time and get this story back with updated impressions details.
 * This only updates the in-memory data.
 *
 * It's recommended to use this method anytime a new impression needs to be recorded for a `PocketSponsoredStory`
 * to ensure values consistency.
 */
fun PocketSponsoredStory.recordNewImpression(): PocketSponsoredStory {
    return this.copy(
        caps = caps.copy(
            currentImpressions = caps.currentImpressions + TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
        ),
    )
}
