/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers;

import android.content.Context;
import android.preference.PreferenceManager;
import android.support.annotation.CallSuper;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.session.SessionManager;

import mozilla.components.utils.ThreadUtils;

import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

public class MainActivityFirstrunTestRule extends ActivityTestRule<MainActivity> {
    private boolean showFirstRun;

    public MainActivityFirstrunTestRule(boolean showFirstRun) {
        super(MainActivity.class);

        this.showFirstRun = showFirstRun;
    }

    @CallSuper
    @Override
    protected void beforeActivityLaunched() {
        super.beforeActivityLaunched();

        Context appContext = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();

        PreferenceManager.getDefaultSharedPreferences(appContext)
                .edit()
                .putBoolean(FIRSTRUN_PREF, !showFirstRun)
                .apply();
    }

    @Override
    protected void afterActivityFinished() {
        super.afterActivityFinished();

        getActivity().finishAndRemoveTask();

        ThreadUtils.INSTANCE.postToMainThread(new Runnable() {
            @Override
            public void run() {
                SessionManager.getInstance().removeAllSessions();
            }
        });
    }
}
