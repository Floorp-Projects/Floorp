/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.focus.helpers

import android.view.ViewConfiguration.getLongPressTimeout
import androidx.annotation.CallSuper
import androidx.test.espresso.intent.rule.IntentsTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import kotlinx.coroutines.runBlocking
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.helpers.TestHelper.getTargetContext
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen

// Basic Test rule with pref to skip the onboarding screen
open class MainActivityFirstrunTestRule(
    launchActivity: Boolean = true,
    private val showFirstRun: Boolean,
    private val showNewOnboarding: Boolean = true,
    private val showStartBrowsingCfrVisibility: Boolean = false,
) : ActivityTestRule<MainActivity>(MainActivity::class.java, launchActivity) {
    private val longTapUserPreference = getLongPressTimeout()
    private val featureSettingsHelper = FeatureSettingsHelper()

    @CallSuper
    override fun beforeActivityLaunched() {
        super.beforeActivityLaunched()
        updateFirstRun(showFirstRun)
        featureSettingsHelper.setShowStartBrowsingCfrEnabled(showStartBrowsingCfrVisibility)
        setNewOnboarding(showNewOnboarding)
        setLongTapTimeout(3000)
    }

    override fun afterActivityFinished() {
        super.afterActivityFinished()

        ThreadUtils.postToMainThread {
            InstrumentationRegistry
                .getInstrumentation()
                .targetContext
                .applicationContext
                .components
                .tabsUseCases
                .removeAllTabs()
        }

        featureSettingsHelper.resetAllFeatureFlags()
        closeNotificationShade()
        setLongTapTimeout(longTapUserPreference)
    }
}

// Test rule that allows usage of Espresso Intents
open class MainActivityIntentsTestRule(launchActivity: Boolean = true, private val showFirstRun: Boolean, private val showStartBrowsingCfrVisibility: Boolean = false) :
    IntentsTestRule<MainActivity>(MainActivity::class.java, launchActivity) {
    private val longTapUserPreference = getLongPressTimeout()
    private val featureSettingsHelper = FeatureSettingsHelper()

    @CallSuper
    override fun beforeActivityLaunched() {
        super.beforeActivityLaunched()

        updateFirstRun(showFirstRun)
        featureSettingsHelper.setShowStartBrowsingCfrEnabled(showStartBrowsingCfrVisibility)
        setLongTapTimeout(3000)
    }

    override fun afterActivityFinished() {
        super.afterActivityFinished()
        ThreadUtils.postToMainThread {
            InstrumentationRegistry
                .getInstrumentation()
                .targetContext
                .applicationContext
                .components
                .tabsUseCases
                .removeAllTabs()
        }

        closeNotificationShade()
        setLongTapTimeout(longTapUserPreference)
    }
}

// Some tests will leave the notification shade open if they fail, needs to be closed before the next tests
private fun closeNotificationShade() {
    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    if (mDevice.findObject(
            UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller"),
        ).exists()
    ) {
        pressBackKey()
    }
}

private fun updateFirstRun(showFirstRun: Boolean) {
    val appContext = InstrumentationRegistry.getInstrumentation()
        .targetContext
        .applicationContext

    val appStore = appContext.components.appStore
    if (appStore.state.screen is Screen.FirstRun && !showFirstRun) {
        hideFirstRun(appStore)
    } else if (appStore.state.screen !is Screen.FirstRun && showFirstRun) {
        showFirstRun(appStore)
    }
    appContext.settings.isFirstRun = showFirstRun
}

private fun showFirstRun(appStore: AppStore) {
    val job = appStore.dispatch(
        AppAction.ShowFirstRun,
    )
    runBlocking { job.join() }
}

private fun hideFirstRun(appStore: AppStore) {
    val job = appStore.dispatch(
        AppAction.FinishFirstRun(tabId = null),
    )
    runBlocking { job.join() }
}

private fun setNewOnboarding(enabled: Boolean) {
    getTargetContext.settings.isNewOnboardingEnable = enabled
}

// changing the device preference for Touch and Hold delay, to avoid long-clicks instead of a single-click
private fun setLongTapTimeout(delay: Int) {
    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    mDevice.executeShellCommand("settings put secure long_press_timeout $delay")
}
