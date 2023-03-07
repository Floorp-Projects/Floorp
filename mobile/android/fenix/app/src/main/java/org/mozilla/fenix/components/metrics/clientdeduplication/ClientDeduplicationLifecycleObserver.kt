/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics.clientdeduplication

import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.nimbus.FxNimbus

/**
 * This observer allows us to mimic roughly the same schedule as the Glean SDK baseline ping.
 * https://github.com/mozilla/glean/blob/main/glean-core/android/src/main/java/mozilla/telemetry/glean/scheduler/GleanLifecycleObserver.kt
 */
class ClientDeduplicationLifecycleObserver(context: Context) : LifecycleEventObserver {
    private val clientDeduplicationPing = ClientDeduplicationPing(context)

    override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
        // The ping will only be sent whenever the Nimbus feature is enabled.
        if (FxNimbus.features.clientDeduplication.value().enabled) {
            when (event) {
                Lifecycle.Event.ON_STOP -> {
                    clientDeduplicationPing.triggerPing(active = false)
                }
                Lifecycle.Event.ON_START -> {
                    // We use ON_START here because we don't want to incorrectly count metrics in
                    // ON_RESUME as pause/resume can happen when interacting with things like the
                    // navigation shade which could lead to incorrectly recording the start of a
                    // duration, etc.
                    //
                    // https://developer.android.com/reference/android/app/Activity.html#onStart()

                    clientDeduplicationPing.triggerPing(active = true)
                }
                else -> {
                    // For other lifecycle events, do nothing
                }
            }
        }
    }
}
