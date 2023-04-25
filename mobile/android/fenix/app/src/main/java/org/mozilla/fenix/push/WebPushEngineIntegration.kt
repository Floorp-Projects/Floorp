/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import android.util.Base64
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webpush.WebPushDelegate
import mozilla.components.concept.engine.webpush.WebPushHandler
import mozilla.components.concept.engine.webpush.WebPushSubscription
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.feature.push.PushScope
import mozilla.components.support.base.log.logger.Logger

/**
 * Engine integration with the push feature to enable WebPush support.
 */
class WebPushEngineIntegration(
    private val engine: Engine,
    private val pushFeature: AutoPushFeature,
    private val coroutineScope: CoroutineScope = MainScope(),
) : AutoPushFeature.Observer {

    private var handler: WebPushHandler? = null
    private val delegate = WebPushEngineDelegate(pushFeature)

    fun start() {
        handler = engine.registerWebPushDelegate(delegate)

        pushFeature.register(this)
    }

    fun stop() {
        pushFeature.unregister(this)
    }

    override fun onMessageReceived(scope: PushScope, message: ByteArray?) {
        coroutineScope.launch {
            handler?.onPushMessage(scope, message)
        }
    }

    override fun onSubscriptionChanged(scope: PushScope) {
        coroutineScope.launch {
            handler?.onSubscriptionChanged(scope)
        }
    }
}

internal class WebPushEngineDelegate(
    private val pushFeature: AutoPushFeature,
) : WebPushDelegate {
    private val logger = Logger("WebPushEngineDelegate")

    override fun onGetSubscription(scope: String, onSubscription: (WebPushSubscription?) -> Unit) {
        // We don't have the appServerKey unless an app is creating a new subscription so we
        // allow the key to be null since it won't be overridden from a previous subscription.
        pushFeature.getSubscription(scope) {
            onSubscription(it?.toEnginePushSubscription())
        }
    }

    override fun onSubscribe(
        scope: String,
        serverKey: ByteArray?,
        onSubscribe: (WebPushSubscription?) -> Unit,
    ) {
        pushFeature.subscribe(
            scope = scope,
            // See the full note at the implementation of `toEnginePushSubscription`.
            // Issue: https://github.com/mozilla/application-services/issues/2698
            appServerKey = null,
            onSubscribeError = {
                logger.error("Error on push onSubscribe.")
                onSubscribe(null)
            },
            onSubscribe = { subscription ->
                onSubscribe(subscription.toEnginePushSubscription())
            },
        )
    }

    override fun onUnsubscribe(scope: String, onUnsubscribe: (Boolean) -> Unit) {
        pushFeature.unsubscribe(
            scope = scope,
            onUnsubscribeError = {
                logger.error("Error on push onUnsubscribe.")
                onUnsubscribe(false)
            },
            onUnsubscribe = {
                onUnsubscribe(it)
            },
        )
    }
}

internal fun AutoPushSubscription.toEnginePushSubscription() = WebPushSubscription(
    scope = this.scope,
    publicKey = this.publicKey.toDecodedByteArray(),
    endpoint = this.endpoint,
    authSecret = this.authKey.toDecodedByteArray(),
    // We don't send the `serverKey` because the code path from that will query
    // the push database for this key, which leads to an exception thrown.
    // Our workaround for now is to not put the server key in to begin with (which
    // will probably break a lot of sites).
    // See: https://github.com/mozilla/application-services/issues/2698
    appServerKey = null,
)

private fun String.toDecodedByteArray() =
    Base64.decode(this.toByteArray(), Base64.URL_SAFE or Base64.NO_PADDING or Base64.NO_WRAP)
