/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test
import android.content.Context
import android.location.LocationManager
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.lifecycle.* // ktlint-disable no-wildcard-imports
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.CoreMatchers.equalTo
import org.hamcrest.core.IsNot.not
import org.json.JSONObject
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.MockLocationProvider

@RunWith(AndroidJUnit4::class)
@LargeTest
class GeolocationTest : BaseSessionTest() {
    private val LOGTAG = "GeolocationTest"
    private val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    private val context = InstrumentationRegistry.getInstrumentation().targetContext
    private lateinit var locManager: LocationManager
    private lateinit var mockGpsProvider: MockLocationProvider
    private lateinit var mockNetworkProvider: MockLocationProvider

    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity { activity ->
            activity.view.setSession(mainSession)
            // Prevents using the network provider for these tests
            sessionRule.setPrefsUntilTestEnd(mapOf("geo.provider.testing" to false))
            locManager = activity.getSystemService(Context.LOCATION_SERVICE) as LocationManager
            mockGpsProvider = sessionRule.MockLocationProvider(locManager, LocationManager.GPS_PROVIDER, 0.0, 0.0, true)
            mockNetworkProvider = sessionRule.MockLocationProvider(locManager, LocationManager.NETWORK_PROVIDER, 0.0, 0.0, true)
        }
    }

    @After
    fun cleanup() {
        try {
            activityRule.scenario.onActivity { activity ->
                activity.view.releaseSession()
            }
            mockGpsProvider.removeMockLocationProvider()
            mockNetworkProvider.removeMockLocationProvider()
        } catch (e: Exception) {}
    }

    private fun setEnableLocationPermissions() {
        sessionRule.delegateDuringNextWait(object : GeckoSession.PermissionDelegate {
            override fun onContentPermissionRequest(
                session: GeckoSession,
                perm: GeckoSession.PermissionDelegate.ContentPermission,
            ):
                GeckoResult<Int> {
                return GeckoResult.fromValue(GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW)
            }
            override fun onAndroidPermissionsRequest(
                session: GeckoSession,
                permissions: Array<out String>?,
                callback: GeckoSession.PermissionDelegate.Callback,
            ) {
                callback.grant()
            }
        })
    }

    private fun getCurrentPositionJS(maximumAge: Number = 0, timeout: Number = 3000, enableHighAccuracy: Boolean = false): JSONObject {
        return mainSession.evaluatePromiseJS(
            """
                    new Promise((resolve, reject) =>
                    window.navigator.geolocation.getCurrentPosition(
                        position => resolve(
                            {latitude: position.coords.latitude,
                            longitude:  position.coords.longitude,
                            accuracy:  position.coords.accuracy}),
                        error => reject(error.code),
                        {maximumAge: $maximumAge,
                         timeout: $timeout,
                         enableHighAccuracy: $enableHighAccuracy }))""",
        ).value as JSONObject
    }

    private fun getCurrentPositionJSWithWait(): JSONObject {
        return mainSession.evaluatePromiseJS(
            """
                new Promise((resolve, reject) =>
                setTimeout(() => {
                    window.navigator.geolocation.getCurrentPosition(
                        position => resolve(
                            {latitude: position.coords.latitude, longitude:  position.coords.longitude})),
                        error => reject(error.code)
                }, "750"))""",
        ).value as JSONObject
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    // General test that location can be requested from JS and that the mock provider is providing location
    @Test
    fun jsContentRequestForLocation() {
        val mockLat = 1.1111
        val mockLon = 2.2222
        mockGpsProvider.setMockLocation(mockLat, mockLon)
        mockGpsProvider.setDoContinuallyPost(true)
        mockGpsProvider.postLocation()
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()
        setEnableLocationPermissions()

        val position = getCurrentPositionJS()
        mockGpsProvider.stopPostingLocation()
        assertThat("Mocked latitude matches.", position["latitude"] as Number, equalTo(mockLat))
        assertThat("Mocked longitude matches.", position["longitude"] as Number, equalTo(mockLon))
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    // Testing that more accurate location providers are selected without high accuracy enabled
    @Test
    fun accurateProviderSelected() {
        val highAccuracy = .000001f
        val highMockLat = 1.1111
        val highMockLon = 2.2222

        // Lower accuracy should still be better than device provider ~20m
        val lowAccuracy = 10.01f
        val lowMockLat = 3.3333
        val lowMockLon = 4.4444

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()
        setEnableLocationPermissions()

        // Test when lower accuracy is more recent
        mockGpsProvider.setMockLocation(highMockLat, highMockLon, highAccuracy)
        mockGpsProvider.setDoContinuallyPost(false)
        mockGpsProvider.postLocation()

        // Sleep ensures the mocked locations have different clock times
        Thread.sleep(10)
        // Set inaccurate second, so that it is the most recent location
        mockNetworkProvider.setMockLocation(lowMockLat, lowMockLon, lowAccuracy)
        mockNetworkProvider.setDoContinuallyPost(false)
        mockNetworkProvider.postLocation()

        val position = getCurrentPositionJS(0, 3000, false)
        assertThat("Higher accuracy latitude is expected.", position["latitude"] as Number, equalTo(highMockLat))
        assertThat("Higher accuracy longitude is expected.", position["longitude"] as Number, equalTo(highMockLon))

        // Test that higher accuracy becomes stale after 6 seconds
        mockGpsProvider.postLocation()
        Thread.sleep(6001)
        mockNetworkProvider.postLocation()
        val inaccuratePosition = getCurrentPositionJS(0, 3000, false)
        assertThat("Lower accuracy latitude is expected.", inaccuratePosition["latitude"] as Number, equalTo(lowMockLat))
        assertThat("Lower accuracy longitude is expected.", inaccuratePosition["longitude"] as Number, equalTo(lowMockLon))
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    // Testing that high accuracy requests a fresh location
    @Test
    fun highAccuracyTest() {
        val accuracyMed = 4f
        val accuracyHigh = .000001f
        val latMedAcc = 1.1111
        val lonMedAcc = 2.2222
        val latHighAcc = 3.3333
        val lonHighAcc = 4.4444

        // High accuracy usage requires HTTPS
        mainSession.loadUri("https://example.com/")
        mainSession.waitForPageStop()
        setEnableLocationPermissions()

        // Have two location providers posting locations
        mockNetworkProvider.setMockLocation(latMedAcc, lonMedAcc, accuracyMed)
        mockNetworkProvider.setDoContinuallyPost(true)
        mockNetworkProvider.postLocation()

        mockGpsProvider.setMockLocation(latHighAcc, lonHighAcc, accuracyHigh)
        mockGpsProvider.setDoContinuallyPost(true)
        mockGpsProvider.postLocation()

        val highAccuracyPosition = getCurrentPositionJS(0, 6001, true)
        mockGpsProvider.stopPostingLocation()
        mockNetworkProvider.stopPostingLocation()

        assertThat("High accuracy latitude is expected.", highAccuracyPosition["latitude"] as Number, equalTo(latHighAcc))
        assertThat("High accuracy longitude is expected.", highAccuracyPosition["longitude"] as Number, equalTo(lonHighAcc))
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    // Checks that location services is reenabled after going to background
    @Test
    fun locationOnBackground() {
        val beforePauseLat = 1.1111
        val beforePauseLon = 2.2222
        val afterPauseLat = 3.3333
        val afterPauseLon = 4.4444
        mockGpsProvider.setDoContinuallyPost(true)

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()
        setEnableLocationPermissions()

        var actualResumeCount = 0
        var actualPauseCount = 0

        // Monitor lifecycle changes
        ProcessLifecycleOwner.get().lifecycle.addObserver(object : DefaultLifecycleObserver {
            override fun onResume(owner: LifecycleOwner) {
                Log.i(LOGTAG, "onResume Event")
                actualResumeCount++
                super.onResume(owner)
                try {
                    mainSession.setActive(true)
                    // onResume is also called when starting too
                    if (actualResumeCount > 1) {
                        // Ensures the location has had time to post
                        Thread.sleep(3001)
                        val onResumeFromPausePosition = getCurrentPositionJS()
                        assertThat("Latitude after onPause matches.", onResumeFromPausePosition["latitude"] as Number, equalTo(afterPauseLat))
                        assertThat("Longitude after onPause matches.", onResumeFromPausePosition["longitude"] as Number, equalTo(afterPauseLon))
                    }
                } catch (e: Exception) {
                    // Intermittent CI test issue where Activity is gone after resume occurs
                    assertThat("onResume count matches.", actualResumeCount, equalTo(2))
                    assertThat("onPause count matches.", actualPauseCount, equalTo(1))
                    try {
                        mockGpsProvider.removeMockLocationProvider()
                    } catch (e: Exception) {
                        // Cleanup could have already occurred
                    }
                }
            }
            override fun onPause(owner: LifecycleOwner) {
                Log.i(LOGTAG, "onPause Event")
                actualPauseCount++
                super.onPause(owner)
                try {
                    mockGpsProvider.setMockLocation(afterPauseLat, afterPauseLon)
                    mockGpsProvider.postLocation()
                } catch (e: Exception) {
                    Log.w(LOGTAG, "onPause was called too late.")
                    // Potential situation where onPause is called too late
                }
            }
        })

        // Before onPause Event
        mockGpsProvider.setMockLocation(beforePauseLat, beforePauseLon)
        mockGpsProvider.postLocation()
        val beforeOnPausePosition = getCurrentPositionJS()
        assertThat("Latitude before onPause matches.", beforeOnPausePosition["latitude"] as Number, equalTo(beforePauseLat))
        assertThat("Longitude before onPause matches.", beforeOnPausePosition["longitude"] as Number, equalTo(beforePauseLon))

        // Ensures a return to the foreground
        Handler(Looper.getMainLooper()).postDelayed({
            sessionRule.requestActivityToForeground(context)
        }, 1500)

        // Will cause onPause event to occur
        sessionRule.simulatePressHome(context)

        // After/During onPause Event
        val whilePausingPosition = getCurrentPositionJSWithWait()
        mockGpsProvider.stopPostingLocation()
        assertThat("Latitude after/during onPause matches.", whilePausingPosition["latitude"] as Number, equalTo(afterPauseLat))
        assertThat("Longitude after/during onPause matches.", whilePausingPosition["longitude"] as Number, equalTo(afterPauseLon))

        assertThat("onResume count matches.", actualResumeCount, equalTo(2))
        assertThat("onPause count matches.", actualPauseCount, equalTo(1))
    }
}
