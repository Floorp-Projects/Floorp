/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.concept.fetch.Client
import mozilla.components.support.base.worker.Frequency
import java.util.UUID
import java.util.concurrent.TimeUnit

internal const val DEFAULT_REFRESH_INTERVAL = 4L
@Suppress("TopLevelPropertyNaming")
internal val DEFAULT_REFRESH_TIMEUNIT = TimeUnit.HOURS

/**
 * Indicating all details for how the pocket stories should be refreshed.
 *
 * @param client [Client] implementation used for downloading the Pocket stories.
 * @param frequency Optional - The interval at which to try and refresh items. Defaults to 1 hour.
 * @param profile Optional - The profile used for downloading sponsored Pocket stories.
 */
class PocketStoriesConfig(
    val client: Client,
    val frequency: Frequency = Frequency(
        DEFAULT_REFRESH_INTERVAL,
        DEFAULT_REFRESH_TIMEUNIT
    ),
    val profile: Profile? = null,
)

/**
 * Sponsored stories configuration data.
 *
 * @param profileId Unique identifier of the user which will be presented with sponsored stories.
 * @param appId Unique identifier of the application using this feature.
 */
class Profile(val profileId: UUID, val appId: String)
