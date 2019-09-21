/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import mozilla.appservices.push.BridgeType
import mozilla.appservices.push.PushAPI
import mozilla.appservices.push.PushManager
import mozilla.appservices.push.SubscriptionResponse
import java.io.Closeable

/**
 * An interface that wraps the [PushAPI].
 *
 * This aides in testing and abstracting out the hurdles of initialization checks required before performing actions
 * on the API.
 */
interface PushConnection : Closeable {
    suspend fun subscribe(channelId: String, scope: String = ""): SubscriptionResponse
    suspend fun unsubscribe(channelId: String): Boolean
    suspend fun unsubscribeAll(): Boolean
    suspend fun updateToken(token: String): Boolean
    suspend fun verifyConnection(): Boolean
    fun decrypt(
        channelId: String,
        body: String,
        encoding: String = "",
        salt: String = "",
        cryptoKey: String = ""
    ): ByteArray
    fun isInitialized(): Boolean
}

internal class RustPushConnection(
    private val databasePath: String,
    private val senderId: String,
    private val serverHost: String,
    private val socketProtocol: Protocol,
    private val serviceType: ServiceType
) : PushConnection {

    @VisibleForTesting
    internal var api: PushAPI? = null

    @GuardedBy("this")
    override suspend fun subscribe(channelId: String, scope: String): SubscriptionResponse = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }
        return pushApi.subscribe(channelId, scope)
    }

    @GuardedBy("this")
    override suspend fun unsubscribe(channelId: String): Boolean = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }
        return pushApi.unsubscribe(channelId)
    }

    @GuardedBy("this")
    override suspend fun unsubscribeAll(): Boolean = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }
        // TODO replace with unsubscribeAll when API exists
        return pushApi.unsubscribe("")
    }

    @GuardedBy("this")
    override suspend fun updateToken(token: String): Boolean = synchronized(this) {
        val pushApi = api
        if (pushApi == null) {
            api = PushManager(
                senderId = senderId,
                serverHost = serverHost,
                httpProtocol = socketProtocol.asString(),
                bridgeType = serviceType.toBridgeType(),
                registrationId = token,
                databasePath = databasePath
            )
            return true
        }
        return pushApi.update(token)
    }

    @GuardedBy("this")
    override suspend fun verifyConnection(): Boolean = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }
        pushApi.verifyConnection()
    }

    @GuardedBy("this")
    override fun decrypt(
        channelId: String,
        body: String,
        encoding: String,
        salt: String,
        cryptoKey: String
    ): ByteArray = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }
        return pushApi.decrypt(channelID = channelId, body = body, encoding = encoding, salt = salt, dh = cryptoKey)
    }

    @GuardedBy("this")
    override fun close() = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }
        pushApi.close()
    }

    override fun isInitialized() = api != null
}

/**
 * Helper function to get the corresponding support [BridgeType] from the support set.
 */
@VisibleForTesting
internal fun ServiceType.toBridgeType() = when (this) {
    ServiceType.FCM -> BridgeType.FCM
    ServiceType.ADM -> BridgeType.ADM
}

@VisibleForTesting
internal fun Protocol.asString() = name.toLowerCase()
