/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.content.pm.ActivityInfo
import android.content.res.Configuration
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.rule.ActivityTestRule

import org.hamcrest.Matchers.*
import org.junit.Rule

import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith

import org.mozilla.geckoview.*
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.OrientationController
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@RunWith(AndroidJUnit4::class)
@MediumTest
class OrientationDelegateTest : BaseSessionTest() {
    val activityRule = ActivityTestRule(GeckoViewTestActivity::class.java, false, true)

    @get:Rule
    override val rules = RuleChain.outerRule(activityRule).around(sessionRule)

    private fun goFullscreen() {
        sessionRule.setPrefsUntilTestEnd(mapOf("full-screen-api.allow-trusted-requests-only" to false))
        mainSession.loadTestPath(FULLSCREEN_PATH)
        mainSession.waitForPageStop()
        val promise = mainSession.evaluatePromiseJS("document.querySelector('#fullscreen').requestFullscreen()")
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override  fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
                assertThat("Div went fullscreen", fullScreen, equalTo(true))
            }
        })
        promise.value
    }

    @Test fun orientationLockedAlready() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.screenorientation.allow-lock" to true))
        goFullscreen()
        // Lock to the current orientation
        if (activityRule.activity.resources.configuration.orientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT){
            sessionRule.delegateUntilTestEnd(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT))
            val promise = mainSession.evaluatePromiseJS("screen.orientation.lock('portrait-primary')")
            sessionRule.waitUntilCalled(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT))
            sessionRule.runtime.orientationChanged(Configuration.ORIENTATION_PORTRAIT)
            promise.value
        } else if (activityRule.activity.resources.configuration.orientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE) {
            sessionRule.delegateUntilTestEnd(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE))
            val promise = mainSession.evaluatePromiseJS("screen.orientation.lock('landscape-primary')")
            sessionRule.waitUntilCalled(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE))
            sessionRule.runtime.orientationChanged(Configuration.ORIENTATION_LANDSCAPE)
            promise.value
        }
    }

    @Test(expected = GeckoSessionTestRule.RejectedPromiseException::class)
    fun orientationLockNoFullscreen() {
        // Verify if fullscreen pre-lock conditions are not met, a rejected promise is returned.
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.screenorientation.allow-lock" to true))
        mainSession.loadTestPath(FULLSCREEN_PATH)
        mainSession.waitForPageStop()
        sessionRule.delegateUntilTestEnd(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE))
        mainSession.evaluateJS("screen.orientation.lock('landscape-primary')")
    }

    @Test fun orientationLock() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.screenorientation.allow-lock" to true))
        goFullscreen()
        // If the orientation is landscape, lock to portrait and wait for delegate. If portrait, lock to landscape instead.
        if (activityRule.activity.resources.configuration.orientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE){
            sessionRule.delegateUntilTestEnd(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT))
            val promise = mainSession.evaluatePromiseJS("screen.orientation.lock('portrait-primary')")
            sessionRule.waitUntilCalled(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT))
            sessionRule.runtime.orientationChanged(Configuration.ORIENTATION_PORTRAIT)
            promise.value
        } else if (activityRule.activity.resources.configuration.orientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) {
            sessionRule.delegateUntilTestEnd(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE))
            val promise = mainSession.evaluatePromiseJS("screen.orientation.lock('landscape-primary')")
            sessionRule.waitUntilCalled(TestOrientationDelegate(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE))
            sessionRule.runtime.orientationChanged(Configuration.ORIENTATION_LANDSCAPE)
            promise.value
        }
    }

    inner class TestOrientationDelegate(private val expectedOrientation : Int) : OrientationController.OrientationDelegate {
        override fun onOrientationLock(aOrientation: Int): GeckoResult<AllowOrDeny>? {
            assertThat("The orientation value is as expected", aOrientation, equalTo(expectedOrientation))
            activityRule.activity.requestedOrientation = aOrientation
            return GeckoResult.allow()
        }
    }
}
