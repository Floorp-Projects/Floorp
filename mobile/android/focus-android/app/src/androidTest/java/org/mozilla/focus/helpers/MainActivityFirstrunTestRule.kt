/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers

import android.preference.PreferenceManager
import android.support.annotation.CallSuper
import android.support.test.InstrumentationRegistry
import android.support.test.rule.ActivityTestRule

import org.mozilla.focus.activity.MainActivity

import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.ext.components

import org.mozilla.focus.fragment.FirstrunFragment.Companion.FIRSTRUN_PREF
import org.mozilla.focus.session.removeAllAndCloseAllSessions

open class MainActivityFirstrunTestRule(private val showFirstRun: Boolean) :
    ActivityTestRule<MainActivity>(MainActivity::class.java) {

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

        activity.finishAndRemoveTask()

        val sessionManager =
            InstrumentationRegistry.getTargetContext().applicationContext.components.sessionManager

        ThreadUtils.postToMainThread(Runnable { sessionManager.removeAllAndCloseAllSessions() })
    }
}
