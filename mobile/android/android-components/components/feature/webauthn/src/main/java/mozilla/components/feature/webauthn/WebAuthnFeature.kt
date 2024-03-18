/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webauthn

import android.app.Activity
import android.content.Intent
import android.content.IntentSender
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.activity.ActivityDelegate
import mozilla.components.support.base.feature.ActivityResultHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger

/**
 * A feature that implementing the [ActivityDelegate] to adds support
 * for [WebAuthn](https://tools.ietf.org/html/rfc8809).
 */
class WebAuthnFeature(
    private val engine: Engine,
    private val activity: Activity,
) : LifecycleAwareFeature, ActivityResultHandler, ActivityDelegate {
    private val logger = Logger("WebAuthnFeature")
    private var requestCodeCounter = ACTIVITY_REQUEST_CODE
    private var callbackRef: ((Intent?) -> Unit)? = null

    override fun start() {
        engine.registerActivityDelegate(this)
    }

    override fun stop() {
        engine.unregisterActivityDelegate()
    }

    override fun onActivityResult(requestCode: Int, data: Intent?, resultCode: Int): Boolean {
        logger.info(
            "Received activity result with " +
                "code: $requestCode " +
                "and original request code: $requestCodeCounter",
        )

        if (requestCode != requestCodeCounter) {
            return false
        }

        requestCodeCounter++

        callbackRef?.invoke(data)

        return true
    }

    override fun startIntentSenderForResult(intent: IntentSender, onResult: (Intent?) -> Unit) {
        logger.info("Received activity delegate request with code: $requestCodeCounter")
        activity.startIntentSenderForResult(intent, requestCodeCounter, null, 0, 0, 0)
        callbackRef = onResult
    }

    companion object {
        const val ACTIVITY_REQUEST_CODE = 10
    }
}
