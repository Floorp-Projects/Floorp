/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.concept.fetch.Client
import mozilla.components.support.base.worker.Frequency
import java.util.UUID
import java.util.concurrent.TimeUnit

internal const val DEFAULT_SPONSORED_STORIES_SITE_ID = "1240699"
internal const val DEFAULT_REFRESH_INTERVAL = 4L
internal const val DEFAULT_SPONSORED_STORIES_REFRESH_INTERVAL = 4L

@Suppress("TopLevelPropertyNaming")
internal val DEFAULT_REFRESH_TIMEUNIT = TimeUnit.HOURS

@Suppress("TopLevelPropertyNaming")
internal val DEFAULT_SPONSORED_STORIES_REFRESH_TIMEUNIT = TimeUnit.HOURS

/**
 * Indicating all details for how the pocket stories should be refreshed.
 *
 * @param client [Client] implementation used for downloading the Pocket stories.
 * @param frequency Optional - The interval at which to try and refresh items. Defaults to 4 hours.
 * @param profile Optional - The profile used for downloading sponsored Pocket stories.
 * @param sponsoredStoriesRefreshFrequency Optional - The interval at which to try and refresh sponsored stories.
 * Defaults to 4 hours.
 * @param sponsoredStoriesParams Optional - Configuration containing parameters used to get the spoc content.
 */
class PocketStoriesConfig(
    val client: Client,
    val frequency: Frequency = Frequency(
        DEFAULT_REFRESH_INTERVAL,
        DEFAULT_REFRESH_TIMEUNIT,
    ),
    val profile: Profile? = null,
    val sponsoredStoriesRefreshFrequency: Frequency = Frequency(
        DEFAULT_SPONSORED_STORIES_REFRESH_INTERVAL,
        DEFAULT_SPONSORED_STORIES_REFRESH_TIMEUNIT,
    ),
    val sponsoredStoriesParams: PocketStoriesRequestConfig = PocketStoriesRequestConfig(),
)

/**
 * Configuration for sponsored stories request indicating parameters used to get spoc content.
 *
 * @property siteId Optional - ID of the site parameter, should be used with care as it changes the
 * set of sponsored stories fetched from the server.
 * @property country Optional - Value of the country parameter, shall be used with care as it allows
 * overriding the IP location and receiving a set of sponsored stories not suited for the real location.
 * @property city Optional - Value of the city parameter, shall be used with care as it allows
 * overriding the IP location and receiving a set of sponsored stories not suited for the real location.
 */
class PocketStoriesRequestConfig(
    val siteId: String = DEFAULT_SPONSORED_STORIES_SITE_ID,
    val country: String = "",
    val city: String = "",
)

/**
 * Sponsored stories configuration data.
 *
 * @param profileId Unique profile identifier which will be presented with sponsored stories.
 * @param appId Unique identifier of the application using this feature.
 */
class Profile(val profileId: UUID, val appId: String)
