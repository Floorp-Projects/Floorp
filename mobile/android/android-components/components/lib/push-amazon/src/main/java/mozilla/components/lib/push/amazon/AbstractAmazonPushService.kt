/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.lib.push.amazon

import android.content.Context
import android.content.Intent
import android.os.Bundle
import com.amazon.device.messaging.ADM
import com.amazon.device.messaging.ADMMessageHandlerBase
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.push.PushService
import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger

/**
 * An Amazon Cloud Messaging implementation of the [PushService] for Android devices that support Google Play Services.
 * [ADMMessageHandlerBase] requires a redundant constructor parameter.
 */
abstract class AbstractAmazonPushService : ADMMessageHandlerBase("AbstractAmazonPushService"), PushService {
    private lateinit var adm: ADM
    private val logger = Logger("AbstractAmazonPushService")

    override fun start(context: Context) {
        adm = ADM(context)
        if (adm.registrationId == null) {
            adm.startRegister()
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public override fun onRegistered(registrationId: String) {
        PushProcessor.requireInstance.onNewToken(registrationId)
    }

    override fun onRegistrationError(errorId: String) {
        PushError.Registration("registration failed: $errorId")
    }

    override fun onUnregistered(registrationId: String) {
        logger.info("account no longer registered with id: $registrationId")
    }

    override fun deleteToken() {
        adm.startUnregister()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public override fun onMessage(intent: Intent) {
        intent.extras?.let {
            try {
                val message = EncryptedPushMessage(
                    channelId = it.getStringWithException("chid"),
                    body = it.getStringWithException("body"),
                    encoding = it.getStringWithException("con"),
                    salt = it.getString("enc"),
                    cryptoKey = it.getString("cryptokey")
                )
                PushProcessor.requireInstance.onMessageReceived(message)
            } catch (e: NoSuchElementException) {
                PushProcessor.requireInstance.onError(
                    PushError.MalformedMessage("parsing encrypted message failed: $e")
                )
            }
        }
    }

    override fun stop() {
        stopSelf()
    }

    private fun Bundle.getStringWithException(key: String): String {
        return getString(key) ?: throw NoSuchElementException(key)
    }
}
