/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity;

import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;

public class EraseShortcutActivity extends Activity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        SessionManager.getInstance().removeAllSessions();

        TelemetryWrapper.eraseShortcutEvent();

        finishAndRemoveTask();
    }
}
