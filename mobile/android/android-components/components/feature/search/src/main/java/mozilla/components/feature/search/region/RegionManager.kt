/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.region

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.search.RegionState
import mozilla.components.service.location.LocationService

// The amount of time (in seconds) we need to be in a new
// location before we update the home region.
// Currently set to 2 weeks.
// https://searchfox.org/mozilla-central/rev/89d33e1c3b0a57a9377b4815c2f4b58d933b7c32/toolkit/modules/Region.jsm#82-85
private const val UPDATE_INTERVAL_MS = 14 * 24 * 60 * 60 * 1000

// The maximum number of times we retry fetching the region from
// the location service until we give up. We will try again on
// the next app start.
private const val MAX_RETRIES = 3

// Timeout until we try to fetch the region again after a failure.
private const val RETRY_TIMEOUT_MS: Long = 10L * 60L * 1000L

private const val PREFERENCE_FILE = "mozac_feature_search_region"
private const val PREFERENCE_KEY_HOME_REGION = "region.home"
private const val PREFERENCE_KEY_CURRENT_REGION = "region.current"
private const val PREFERENCE_KEY_REGION_FIRST_SEEN = "region.first_seen"

/**
 * Internal RegionManager for keeping track of the "current" and "home" region of a user. Used by
 * [RegionMiddleware].
 */
internal class RegionManager(
    context: Context,
    private val locationService: LocationService,
    private val currentTime: () -> Long = { System.currentTimeMillis() },
    private val preferences: Lazy<SharedPreferences> = lazy {
        context.getSharedPreferences(
            PREFERENCE_FILE,
            Context.MODE_PRIVATE,
        )
    },
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO,
) {
    private var homeRegion: String?
        get() = preferences.value.getString(PREFERENCE_KEY_HOME_REGION, null)
        set(value) = preferences.value.edit().putString(PREFERENCE_KEY_HOME_REGION, value).apply()

    private var currentRegion: String?
        get() = preferences.value.getString(PREFERENCE_KEY_CURRENT_REGION, null)
        set(value) = preferences.value.edit().putString(PREFERENCE_KEY_CURRENT_REGION, value).apply()

    private var firstSeen: Long?
        get() = preferences.value.getLong(PREFERENCE_KEY_REGION_FIRST_SEEN, 0)
        set(value) = if (value == null) {
            preferences.value.edit().remove(PREFERENCE_KEY_REGION_FIRST_SEEN).apply()
        } else {
            preferences.value.edit().putLong(PREFERENCE_KEY_REGION_FIRST_SEEN, value).apply()
        }

    fun region(): RegionState? {
        return homeRegion?.let { region ->
            RegionState(
                region,
                currentRegion ?: region,
            )
        }
    }

    suspend fun update(): RegionState? {
        val region = fetchRegionWithRetry()?.countryCode ?: return null

        if (homeRegion == null) {
            // If we do not have a value for the home region yet, then we can set it immediately.
            homeRegion = region
            return RegionState(home = region, current = region)
        }

        return when (region) {
            homeRegion -> {
                // If we are in the home region (again) then we can clear a previously seen different
                // "current" region.
                currentRegion = null
                firstSeen = null
                null
            }

            currentRegion -> {
                val now = currentTime()
                if (now > (firstSeen ?: 0) + UPDATE_INTERVAL_MS) {
                    // We have been in the "current" region longer than the specified "interval".
                    // So we will set the "current" region as our new home region.
                    homeRegion = region
                    RegionState(home = region, current = region)
                } else {
                    null
                }
            }

            else -> {
                // This region is neither the home region nor the current region. We set it as the
                // new "current" region and remember when we saw it the first time.
                firstSeen = currentTime()
                currentRegion = region
                null
            }
        }
    }

    private suspend fun fetchRegionWithRetry(): LocationService.Region? = withContext(dispatcher) {
        repeat(MAX_RETRIES) {
            val region = locationService.fetchRegion(readFromCache = false)
            if (region != null) {
                return@withContext region
            }
            delay(RETRY_TIMEOUT_MS)
        }
        return@withContext null
    }
}
