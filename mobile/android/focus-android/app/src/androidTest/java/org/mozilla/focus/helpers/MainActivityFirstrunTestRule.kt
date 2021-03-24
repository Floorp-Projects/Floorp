/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers

import androidx.annotation.CallSuper
import androidx.preference.PreferenceManager
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.fragment.FirstrunFragment.Companion.FIRSTRUN_PREF

open class MainActivityFirstrunTestRule(launchActivity: Boolean = true, private val showFirstRun: Boolean) :
    ActivityTestRule<MainActivity>(MainActivity::class.java, launchActivity) {

    @CallSuper
    override fun beforeActivityLaunched() {
        super.beforeActivityLaunched()

        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext

        PreferenceManager.getDefaultSharedPreferences(appContext)
            .edit()
            .putBoolean(FIRSTRUN_PREF, !showFirstRun)
            .apply()
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
    }
}
