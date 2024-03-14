/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.content.Context
import android.location.Location
import android.location.LocationManager
import android.os.Build
import android.os.SystemClock
import android.util.Log
import androidx.test.core.app.ApplicationProvider
import org.junit.rules.ExternalResource
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.mDevice
import java.util.Date
import kotlin.random.Random

private const val mockProviderName = LocationManager.GPS_PROVIDER

/**
 * Rule that sets up a mock location provider that can inject location samples
 * straight to the device that the test is running on.
 *
 * Credit to the mapbox team
 * https://github.com/mapbox/mapbox-navigation-android/blob/87fab7ea1152b29533ee121eaf6c05bc202adf02/libtesting-ui/src/main/java/com/mapbox/navigation/testing/ui/MockLocationUpdatesRule.kt
 *
 */
class MockLocationUpdatesRule : ExternalResource() {
    private val appContext = (ApplicationProvider.getApplicationContext() as Context)
    val latitude = Random.nextDouble(-90.0, 90.0)
    val longitude = Random.nextDouble(-180.0, 180.0)

    private val locationManager: LocationManager by lazy {
        (appContext.getSystemService(Context.LOCATION_SERVICE) as LocationManager)
    }

    override fun before() {
        Log.i(TAG, "MockLocationUpdatesRule: Trying to enable the mock location setting on the device")
        /* ADB command to enable the mock location setting on the device.
         * Will not be turned back off due to limitations on knowing its initial state.
         */
        Log.i(TAG, "MockLocationUpdatesRule: Trying to execute cmd \"appops set ${appContext.packageName} android:mock_location allow\"")
        mDevice.executeShellCommand(
            "appops set " +
                appContext.packageName +
                " android:mock_location allow",
        )
        Log.i(TAG, "MockLocationUpdatesRule: Executed cmd \"appops set ${appContext.packageName} android:mock_location allow\"")
        // To mock locations we need a location provider, so we generate and set it here.
        try {
            locationManager.addTestProvider(
                mockProviderName,
                false,
                false,
                false,
                false,
                true,
                true,
                true,
                3,
                2,
            )
        } catch (ex: Exception) {
            // unstable
            Log.i(TAG, "MockLocationUpdatesRule: Exception $ex caught, addTestProvider failed")
        }
        locationManager.setTestProviderEnabled(mockProviderName, true)
        Log.i(TAG, "MockLocationUpdatesRule: Enabled the mock location setting on the device")
    }

    // Cleaning up the location provider after the test.
    override fun after() {
        Log.i(TAG, "MockLocationUpdatesRule: Trying to clean up the location provider")
        locationManager.setTestProviderEnabled(mockProviderName, false)
        locationManager.removeTestProvider(mockProviderName)
        Log.i(TAG, "MockLocationUpdatesRule: Cleaned up the location provider")
    }

    /**
     * Generate a valid mock location data and set with the help of a test provider.
     *
     * @param modifyLocation optional callback for modifying the constructed location before setting it.
     */
    fun setMockLocation(modifyLocation: (Location.() -> Unit)? = null) {
        Log.i(TAG, "setMockLocation: Trying to set the mock location")
        check(Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            "MockLocationUpdatesRule is supported only on Android devices " +
                "running version >= Build.VERSION_CODES.M"
        }

        val location = Location(mockProviderName)
        location.time = Date().time
        location.elapsedRealtimeNanos = SystemClock.elapsedRealtimeNanos()
        location.accuracy = 5f
        location.altitude = 0.0
        location.bearing = 0f
        location.speed = 5f
        location.latitude = latitude
        location.longitude = longitude
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            location.verticalAccuracyMeters = 5f
            location.bearingAccuracyDegrees = 5f
            location.speedAccuracyMetersPerSecond = 5f
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            location.elapsedRealtimeUncertaintyNanos = 0.0
        }

        modifyLocation?.let {
            location.apply(it)
        }

        locationManager.setTestProviderLocation(mockProviderName, location)
        Log.i(TAG, "setMockLocation: The mock location was successfully set")
    }
}
