/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.app.Activity
import android.content.Intent
import android.content.IntentSender
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.activity.ActivityDelegate
import mozilla.components.support.base.feature.ActivityResultHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger

/**
 * This implementation of the WebAuthnFeature is only for testing in a nightly signed build.
 *
 * ⚠️ This should always be behind the [FeatureFlags.webAuthFeature] nightly flag.
 */
class WebAuthnFeature(
    private val engine: Engine,
    private val activity: Activity
) : LifecycleAwareFeature, ActivityResultHandler {
    val logger = Logger("WebAuthnFeature")
    var requestCode = ACTIVITY_REQUEST_CODE
    var resultCallback: ((Intent?) -> Unit)? = null
    private val delegate = object : ActivityDelegate {
        override fun startIntentSenderForResult(intent: IntentSender, onResult: (Intent?) -> Unit) {
            val code = requestCode++
            logger.info("Received activity delegate request with code: $code intent: $intent")
            activity.startIntentSenderForResult(intent, code, null, 0, 0, 0)
            resultCallback = onResult
        }
    }

    override fun start() {
        logger.info("Feature started.")
        engine.registerActivityDelegate(delegate)
    }

    override fun stop() {
        logger.info("Feature stopped.")
        engine.unregisterActivityDelegate()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?): Boolean {
        logger.info("Received activity result with code: $requestCode\ndata: $data")
        if (this.requestCode == requestCode) {
            logger.info("Invoking callback!")
            resultCallback?.invoke(data)
            return true
        }

        return false
    }

    companion object {
        const val ACTIVITY_REQUEST_CODE = 10
    }
}
