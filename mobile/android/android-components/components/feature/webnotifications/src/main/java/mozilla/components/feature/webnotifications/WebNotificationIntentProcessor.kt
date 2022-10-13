/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.content.Intent
import android.os.Parcelable
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.intent.processing.IntentProcessor

/**
 * Intent processor that tries matching a web notification and delegating a click interaction with it.
 */
class WebNotificationIntentProcessor(
    private val engine: Engine,
) : IntentProcessor {
    /**
     * Processes an incoming intent expected to contain information about a web notification.
     * If such information is available this will inform the web notification about it being clicked.
     */
    override fun process(intent: Intent): Boolean {
        @Suppress("MoveVariableDeclarationIntoWhen")
        val engineNotification = intent.extras?.get(NativeNotificationBridge.EXTRA_ON_CLICK) as? Parcelable

        return when (engineNotification) {
            null -> false
            else -> {
                engine.handleWebNotificationClick(engineNotification)
                true
            }
        }
    }
}
