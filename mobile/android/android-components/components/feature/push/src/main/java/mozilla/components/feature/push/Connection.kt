/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import android.content.Context
import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import mozilla.appservices.push.BridgeType
import mozilla.appservices.push.PushException.GeneralException
import mozilla.appservices.push.PushManager
import mozilla.appservices.push.SubscriptionResponse
import java.io.Closeable
import java.io.File
import java.util.Locale
import java.util.UUID
import mozilla.appservices.push.PushSubscriptionChanged as SubscriptionChanged

typealias PushScope = String
typealias AppServerKey = String

/**
 * An interface that wraps the [PushManager].
 *
 * This aides in testing and abstracting out the hurdles of initialization checks required before performing actions
 * on the API.
 */
interface PushConnection : Closeable {
    /**
     * Creates a push subscription for the given scope.
     *
     * @return A push subscription.
     */
    suspend fun subscribe(scope: PushScope, appServerKey: AppServerKey? = null): AutoPushSubscription

    /**
     * Un-subscribes a push subscription for the given scope.
     *
     * @return the invocation result if it was successful.
     */
    suspend fun unsubscribe(scope: PushScope): Boolean

    /**
     * Un-subscribes all push subscriptions.
     */
    suspend fun unsubscribeAll()

    /**
     * Returns `true` if the specified [scope] has a subscription.
     */
    suspend fun containsSubscription(scope: PushScope): Boolean

    /**
     * Updates the registration token to the native Push API if it changes.
     *
     * @return the invocation result if it was successful.
     */
    suspend fun updateToken(token: String): Boolean

    /**
     * Checks validity of current push subscriptions.
     *
     * @return the list of subscriptions that have expired which can be used to notify subscribers.
     */
    suspend fun verifyConnection(): List<AutoPushSubscriptionChanged>

    /**
     * Decrypts a received message.
     *
     * @return a pair of the push scope and the decrypted message, respectively, else null if there was no valid client
     * for the message.
     */
    suspend fun decryptMessage(
        channelId: String,
        body: String?,
        encoding: String = "",
        salt: String = "",
        cryptoKey: String = "",
    ): DecryptedMessage?

    /**
     * Checks if the native Push API has already been initialized.
     */
    fun isInitialized(): Boolean
}

/**
 * An implementation of [PushConnection] for the native component using the [PushAPI].
 *
 * Implementation notes: There are few important concepts to note here - for WebPush, the identifier that is required
 * for notifying clients is called a "scope". This can be a site's host URL or an other uniquely identifying string to
 * create a push subscription for that given scope.
 *
 * In the native Rust component the concept is similar, in that we have a unique ID for each subscription that is local
 * to the device, however the ID is called a "channel ID" (or chid), which is a UUID. In the implementation, the scope
 * can also be provided to the native layer, however currently it is just stored in the database but not used in any
 * significant way (this may change in the future).
 *
 * With this in mind, we decided to write our implementation to be a 1-1 mapping of the public API 'scope' to the
 * internal API 'channel ID' by generating a UUID from the scope value. This means that we have a reproducible way to
 * always retrieve the channel ID when the caller passes the scope to us.
 *
 * Some nuances are that we also need to provide the native API the same scope value along with the
 * scope-based channel ID value to satisfy the internal API requirements, at the cost of some noticeable duplication of
 * information, so that we can retrieve those values later; see [RustPushConnection.subscribe] and
 * [RustPushConnection.unsubscribe] implementations for details.
 *
 * Another nuance is that, in order to satisfy the API shape of the subscription, we need to query the database for
 * the scope in accordance with decrypting the message. This lets us notify our observers on messages for a particular
 * receiver; see [RustPushConnection.decryptMessage] implementation for details.
 */
internal class RustPushConnection(
    context: Context,
    private val senderId: String,
    private val serverHost: String,
    private val socketProtocol: Protocol,
    private val serviceType: ServiceType,
) : PushConnection {

    private val databasePath by lazy { File(context.filesDir, DB_NAME).canonicalPath }

    @VisibleForTesting
    internal var api: PushManager? = null

    @GuardedBy("this")
    override suspend fun subscribe(
        scope: PushScope,
        appServerKey: AppServerKey?,
    ): AutoPushSubscription = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        // Generate the channel ID from the scope so that it's reproducible if we need to query for it later.
        val channelId = scope.toChannelId()
        val response = pushApi.subscribe(channelId, scope, appServerKey)

        return response.toPushSubscription(scope, appServerKey)
    }

    @GuardedBy("this")
    override suspend fun unsubscribe(scope: PushScope): Boolean = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        // Generate the channel ID from the scope so that it's reproducible if we need to query for it later.
        val channelId = scope.toChannelId()

        return pushApi.unsubscribe(channelId)
    }

    @GuardedBy("this")
    override suspend fun unsubscribeAll(): Unit = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        pushApi.unsubscribeAll()
    }

    @GuardedBy("this")
    override suspend fun containsSubscription(scope: PushScope): Boolean = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        return pushApi.dispatchInfoForChid(scope.toChannelId()) != null
    }

    @GuardedBy("this")
    override suspend fun updateToken(token: String): Boolean = synchronized(this) {
        var pushApi = api
        if (pushApi == null) {
            api = PushManager(
                senderId = senderId,
                serverHost = serverHost,
                httpProtocol = socketProtocol.asString(),
                bridgeType = serviceType.toBridgeType(),
                registrationId = token,
                databasePath = databasePath,
            )
            pushApi = api!!
            // This may be a new token, so we must tell the server about it even if this is the
            // push being initialized for the first time in this process.
        }
        // This call will fail if we haven't 'subscribed' yet.
        return try {
            pushApi.update(token)
        } catch (e: GeneralException) {
            val fakeChannelId = "fake".toChannelId()
            // It's possible that we have a race (on a first run) between 'subscribing' and setting a token.
            // (Or even more likely is that we have no subscriptions, but are still trying to
            // initialize this service with a new FCM token)
            // 'update' expects that we've called 'subscribe' (which would obtain a 'uaid' from an autopush
            // server), which we need to have in order to call 'update' on the library.
            // Note also: this work-around means we are making 2 connections to the push server
            // even when there are zero subscriptions actually desired, so it's wildly inefficient
            // for the majority of users.
            // In https://github.com/mozilla/application-services/issues/2490 this will be fixed, and we
            // can clean up this work-around.
            pushApi.subscribe(fakeChannelId)

            // If this fails again, give up - it seems like a legit failure that we should re-throw.
            pushApi.update(token)
        }
    }

    @GuardedBy("this")
    override suspend fun verifyConnection(): List<AutoPushSubscriptionChanged> = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        return pushApi.verifyConnection().map { it.toPushSubscriptionChanged() }
    }

    @GuardedBy("this")
    override suspend fun decryptMessage(
        channelId: String,
        body: String?,
        encoding: String,
        salt: String,
        cryptoKey: String,
    ): DecryptedMessage? = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        // Query for the scope so that we can notify observers who the decrypted message is for.
        val scope = pushApi.dispatchInfoForChid(channelId)?.scope

        if (scope != null) {
            if (body == null) {
                return DecryptedMessage(scope, null)
            }

            val data = pushApi.decrypt(
                channelId = channelId,
                body = body,
                encoding = encoding,
                salt = salt,
                dh = cryptoKey,
            )

            return DecryptedMessage(scope, data.toByteArray())
        } else {
            return null
        }
    }

    @GuardedBy("this")
    override fun close() = synchronized(this) {
        val pushApi = api
        check(pushApi != null) { "Rust API is not initiated; updateToken hasn't been called yet." }

        pushApi.close()
    }

    @GuardedBy("this")
    override fun isInitialized() = synchronized(this) { api != null }

    companion object {
        internal const val DB_NAME = "push.sqlite"
    }
}

/**
 * Helper function to get the corresponding support [BridgeType] from the support set.
 */
@VisibleForTesting
internal fun ServiceType.toBridgeType() = when (this) {
    ServiceType.FCM -> BridgeType.FCM
}

/**
 * Helper function to convert the [Protocol] into the required value the native implementation requires.
 */
@VisibleForTesting
internal fun Protocol.asString() = name.lowercase(Locale.ROOT)

/**
 * A channel ID from the provided scope.
 */
internal fun PushScope.toChannelId() =
    UUID.nameUUIDFromBytes(this.toByteArray()).toString().replace("-", "")

/**
 * A helper to convert the internal data class.
 */
internal fun SubscriptionResponse.toPushSubscription(
    scope: String,
    appServerKey: AppServerKey? = null,
): AutoPushSubscription {
    return AutoPushSubscription(
        scope = scope,
        endpoint = subscriptionInfo.endpoint,
        authKey = subscriptionInfo.keys.auth,
        publicKey = subscriptionInfo.keys.p256dh,
        appServerKey = appServerKey,
    )
}

/**
 * A helper to convert the internal data class.
 */
internal fun SubscriptionChanged.toPushSubscriptionChanged() = AutoPushSubscriptionChanged(
    scope = scope,
    channelId = channelId,
)

/**
 * Represents a decrypted push message for notifying observers of the [scope].
 */
data class DecryptedMessage(val scope: PushScope, val message: ByteArray?) {

    // Generated; do not edit.
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as DecryptedMessage

        if (scope != other.scope) return false
        if (message != null) {
            if (other.message == null) return false
            if (!message.contentEquals(other.message)) return false
        } else if (other.message != null) return false

        return true
    }

    // Generated; do not edit.
    @Suppress("MagicNumber")
    override fun hashCode(): Int {
        var result = scope.hashCode()
        result = 31 * result + (message?.contentHashCode() ?: 0)
        return result
    }
}
