/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test
import android.content.Context
import android.content.Intent
import android.location.LocationManager
import android.os.Handler
import android.os.Looper
import androidx.lifecycle.*
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.CoreMatchers.equalTo
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


@RunWith(AndroidJUnit4::class)
 @LargeTest
 class GeolocationTest : BaseSessionTest() {
    private val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    private val locProvider = "mockTestLocationProvider";
    private val context = InstrumentationRegistry.getInstrumentation().targetContext
    private lateinit var locManager : LocationManager
    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity { activity ->
            activity.view.setSession(mainSession)
            // Prevents using the network provider for these tests
            sessionRule.setPrefsUntilTestEnd(mapOf("geo.provider.testing" to false))
            locManager = activity.getSystemService(Context.LOCATION_SERVICE) as LocationManager
            sessionRule.addMockLocationProvider(locManager, locProvider)
        }
    }

    @After
    fun cleanup() {
        try {
            activityRule.scenario.onActivity { activity ->
                activity.view.releaseSession()
            }
        } catch (e : Exception){}
    }

    private fun setEnableLocationPermissions(){
        sessionRule.delegateDuringNextWait(object : GeckoSession.PermissionDelegate {
            override fun onContentPermissionRequest(
                    session: GeckoSession, perm: GeckoSession.PermissionDelegate.ContentPermission):
                    GeckoResult<Int> {
                return GeckoResult.fromValue(GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW)
            }
            override fun onAndroidPermissionsRequest(
                    session: GeckoSession, permissions: Array<out String>?,
                    callback: GeckoSession.PermissionDelegate.Callback) {
                callback.grant()
            }
        })
    }

    private fun pressHome() {
        val intent = Intent();
        intent.action = Intent.ACTION_MAIN;
        intent.addCategory(Intent.CATEGORY_HOME);
        context.startActivity(intent);
    }

    private fun setActivityToForeground() {
        val notificationIntent = Intent(context, GeckoViewTestActivity::class.java)
        notificationIntent.action = Intent.ACTION_MAIN;
        notificationIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        context.startActivity(notificationIntent)
    }

    private fun getCurrentPositionJS(): JSONObject {
         return mainSession.evaluatePromiseJS("""
                    new Promise((resolve, reject) =>
                    window.navigator.geolocation.getCurrentPosition(
                        position => resolve(
                            {latitude: position.coords.latitude, longitude:  position.coords.longitude})),
                        error => reject(error.code))""").value as JSONObject
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    // General test that location can be requested from JS and that the mock provider is providing location
    @Test fun jsContentRequestForLocation() {
        val mockLat = 1.1111
        val mockLon = 2.2222
        sessionRule.setMockLocation(locManager, locProvider, mockLat, mockLon)
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()
        setEnableLocationPermissions()

        val position = getCurrentPositionJS()
        assertThat("Mocked latitude matches.", position["latitude"] as Number, equalTo(mockLat))
        assertThat("Mocked longitude matches.", position["longitude"] as Number, equalTo(mockLon))

    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    // Checks that location services is reenabled after going to background
     @Test fun locationOnBackground() {
        val beforePauseLat = 1.1111
        val beforePauseLon = 2.2222
        val afterPauseLat = 3.3333
        val afterPauseLon = 4.4444

        sessionRule.setMockLocation(locManager, locProvider, beforePauseLat, beforePauseLon)
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()
        setEnableLocationPermissions()

        var actualResumeCount = 0
        var actualPauseCount = 0

        // Monitor lifecycle changes
        ProcessLifecycleOwner.get().lifecycle.addObserver(object: DefaultLifecycleObserver {
            override fun onResume(owner: LifecycleOwner) {
                actualResumeCount++;
                super.onResume(owner)
                try {
                    mainSession.setActive(true)
                    // onResume is also called when starting too
                    if(actualResumeCount > 1) {
                        val onResumeFromPausePosition = getCurrentPositionJS()
                        assertThat("Latitude after onPause matches.", onResumeFromPausePosition["latitude"] as Number, equalTo(afterPauseLat))
                        assertThat("Longitude after onPause matches.", onResumeFromPausePosition["longitude"] as Number, equalTo(afterPauseLon))
                    }
                } catch (e : Exception) {
                    // Intermittent CI test issue where Activity is gone after resume occurs
                    assertThat("onResume count matches.", actualResumeCount, equalTo(2))
                    assertThat("onPause count matches.", actualPauseCount, equalTo(1))
                    try {
                        sessionRule.removeMockLocationProvider(locManager, locProvider)
                    } catch (e: Exception) {
                        // Cleanup could have already occurred
                    }
                }
            }
            override fun onPause(owner: LifecycleOwner) {
                actualPauseCount ++;
                super.onPause(owner)
                try {
                    sessionRule.setMockLocation(locManager, locProvider, afterPauseLat, afterPauseLon)
                } catch (e: Exception) {
                    // Potential situation where onPause is called too late
                }
            }
        })

        // Before onPause Event
        val beforeOnPausePosition = getCurrentPositionJS()
        assertThat("Latitude before onPause matches.", beforeOnPausePosition["latitude"] as Number, equalTo(beforePauseLat))
        assertThat("Longitude before onPause matches.", beforeOnPausePosition["longitude"] as Number, equalTo(beforePauseLon))

        // Ensures a return to the foreground
        Handler(Looper.getMainLooper()).postDelayed({
            setActivityToForeground()
        }, 1000)

        // Will cause onPause event to occur
        pressHome()

        // After/During onPause Event
        val whilePausingPosition = getCurrentPositionJS()
        assertThat("Longitude after/during onPause matches.", whilePausingPosition["latitude"] as Number, equalTo(afterPauseLat))
        assertThat("Longitude after/during onPause matches.", whilePausingPosition["longitude"] as Number, equalTo(afterPauseLon))

        assertThat("onResume count matches.", actualResumeCount, equalTo(2))
        assertThat("onPause count matches.", actualPauseCount, equalTo(1))
    }
}



