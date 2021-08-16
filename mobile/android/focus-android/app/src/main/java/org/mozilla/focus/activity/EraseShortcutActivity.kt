/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.app.Activity
import android.os.Bundle
import org.mozilla.focus.ext.components
import org.mozilla.focus.telemetry.TelemetryWrapper

class EraseShortcutActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        components.tabsUseCases.removeAllTabs()

        TelemetryWrapper.eraseShortcutEvent()

        finishAndRemoveTask()
    }
}
