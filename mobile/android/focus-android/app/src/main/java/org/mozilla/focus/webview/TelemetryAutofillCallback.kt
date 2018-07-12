/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.webview

import android.annotation.TargetApi
import android.content.Context
import android.os.Build
import android.view.View
import android.view.autofill.AutofillManager
import org.mozilla.focus.telemetry.TelemetryWrapper

/**
 * Callback implementation to send a telemetry event whenever an autocomplete input is shown.
 */
@TargetApi(Build.VERSION_CODES.O)
object TelemetryAutofillCallback : AutofillManager.AutofillCallback() {
    fun register(context: Context) {
        context.getSystemService(AutofillManager::class.java)
            .registerCallback(TelemetryAutofillCallback)
    }

    fun unregister(context: Context) {
        context.getSystemService(AutofillManager::class.java)
                .unregisterCallback(TelemetryAutofillCallback)
    }

    override fun onAutofillEvent(view: View?, virtualId: Int, event: Int) {
        super.onAutofillEvent(view, virtualId, event)

        if (event == EVENT_INPUT_SHOWN) {
            TelemetryWrapper.autofillShownEvent()
        }
    }
}
