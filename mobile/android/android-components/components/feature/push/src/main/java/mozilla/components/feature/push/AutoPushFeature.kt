/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.push

import android.content.Context
import android.content.SharedPreferences
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import mozilla.appservices.push.PushError as RustPushError
import mozilla.appservices.push.SubscriptionResponse
import mozilla.components.concept.push.Bus
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.push.PushService
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.File
import java.util.UUID
import java.util.concurrent.Executors
import kotlin.coroutines.CoroutineContext

/**
 * A implementation of a [PushProcessor] that should live as a singleton by being installed
 * in the Application's onCreate. It receives messages from a service and forwards them
 * to be decrypted and routed.
 *
 * <code>
 *     class Application {
 *          override fun onCreate() {
 *              val feature = AutoPushFeature(context, service, configuration)
 *              PushProvider.install(push)
 *          }
 *     }
 * </code>
 *
 * Listen for subscription information changes for each registered [PushType]:
 *
 * <code>
 *    feature.registerForSubscriptions(object: Bus.Observer<PushType, String> {
 *        override fun onSubscriptionAvailable(subscription: AutoPushSubscription) { }
 *    })
 *    feature.subscribeForType(PushType.Services)
 * </code>
 *
 * Listen also for push messages for each registered [PushType]:
 *
 * <code>
 *     feature.registerForPushMessages(object: Bus.Observer<PushType, String> {
 *        override fun onEvent(message: String) { }
 *     })
 * </code>
 *
 */
@Suppress("TooManyFunctions")
class AutoPushFeature(
    private val context: Context,
    private val service: PushService,
    config: PushConfig,
    coroutineContext: CoroutineContext = Executors.newSingleThreadExecutor().asCoroutineDispatcher(),
    private val connection: PushConnection = RustPushConnection(
        senderId = config.senderId,
        serverHost = config.serverHost,
        socketProtocol = config.protocol,
        serviceType = config.serviceType,
        databasePath = File(context.filesDir, DB_NAME).canonicalPath
    )
) : PushProcessor {

    private val logger = Logger("AutoPushFeature")
    private val subscriptionObservers: Observable<PushSubscriptionObserver> = ObserverRegistry()
    private val messageObserverBus: Bus<PushType, String> = MessageBus()
    // The preference that stores new registration tokens.
    private val prefToken: String?
        get() = preferences(context).getString(PREF_TOKEN, null)

    internal var job: Job = SupervisorJob()
    private val scope = CoroutineScope(coroutineContext) + job

    init {
        // If we have a token, initialize the rust component first.
        prefToken?.let { token ->
            scope.launch {
                connection.updateToken(token)
            }
        }
    }

    /**
     * Starts the push service provided.
     */
    override fun initialize() { service.start(context) }

    /**
     * Un-subscribes from all push message channels and stops the push service.
     * This should only be done on an account logout or app data deletion.
     */
    override fun shutdown() {
        service.stop()

        DeliveryManager.with(connection) {
            scope.launch {
                // TODO replace with unsubscribeAll API when available
                PushType.values().forEach { type ->
                    unsubscribe(type.toChannelId())
                }
                job.cancel()
            }
        }
    }

    /**
     * New registration tokens are received and sent to the Autopush server which also performs subscriptions for
     * each push type and notifies the subscribers.
     */
    override fun onNewToken(newToken: String) {
        scope.launchAndTry {
            connection.updateToken(newToken)

            // Subscribe all only if this is the first time.
            if (prefToken.isNullOrEmpty()) {
                subscribeAll()
            }

            saveToken(context, newToken)
        }
    }

    /**
     * New encrypted messages received from a supported push messaging service.
     */
    override fun onMessageReceived(message: EncryptedPushMessage) {
        scope.launchAndTry {
            val type = DeliveryManager.serviceForChannelId(message.channelId)
            DeliveryManager.with(connection) {
                val decrypted = decrypt(
                    channelId = message.channelId,
                    body = message.body,
                    encoding = message.encoding,
                    salt = message.salt,
                    cryptoKey = message.cryptoKey
                )
                messageObserverBus.notifyObservers(type, String(decrypted))
            }
        }
    }

    override fun onError(error: PushError) {
        // Only log errors for now.
        logger.error("${error.javaClass.simpleName} error: ${error.desc}")
    }

    /**
     * Register to receive push subscriptions when requested or when they have been re-registered.
     *
     * @param observer the observer that will be notified.
     * @param owner the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner].
     * @param autoPause whether to stop notifying the observer during onPause lifecycle events.
     * Defaults to false so that subscriptions are always delivered to observers.
     */
    fun registerForSubscriptions(
        observer: PushSubscriptionObserver,
        owner: LifecycleOwner = ProcessLifecycleOwner.get(),
        autoPause: Boolean = false
    ) = subscriptionObservers.register(observer, owner, autoPause)

    /**
     * Register to receive push messages for the associated [PushType].
     *
     * @param type the push message type that you want to be registered.
     * @param observer the observer that will be notified.
     * @param owner the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner].
     * @param autoPause whether to stop notifying the observer during onPause lifecycle events.
     * Defaults to false so that messages are always delivered to observers.
     */
    fun registerForPushMessages(
        type: PushType,
        observer: Bus.Observer<PushType, String>,
        owner: LifecycleOwner = ProcessLifecycleOwner.get(),
        autoPause: Boolean = false
    ) = messageObserverBus.register(type, observer, owner, autoPause)

    /**
     * Notifies observers about the subscription information for the push type if available.
     */
    fun subscribeForType(type: PushType) {
        DeliveryManager.with(connection) {
            scope.launchAndTry {
                val sub = subscribe(type.toChannelId()).toPushSubscription()
                subscriptionObservers.notifyObservers { onSubscriptionAvailable(sub) }
            }
        }
    }

    /**
     * Returns subscription information for the push type if available.
     *
     * Implementation notes: We need to connect this to the device constellation so that we update our subscriptions
     * when notified by FxA. See [#3859][0].
     *
     * [0]: https://github.com/mozilla-mobile/android-components/issues/3859
     */
    fun unsubscribeForType(type: PushType) {
        DeliveryManager.with(connection) {
            scope.launchAndTry {
                unsubscribe(type.toChannelId())
            }
        }
    }

    /**
     * Returns all subscription for the push type if available.
     */
    fun subscribeAll() {
        DeliveryManager.with(connection) {
            scope.launchAndTry {
                PushType.values().forEach { type ->
                    val sub = subscribe(type.toChannelId()).toPushSubscription()
                    subscriptionObservers.notifyObservers { onSubscriptionAvailable(sub) }
                }
            }
        }
    }

    /**
     * Deletes the registration token locally so that it forces the service to get a new one the
     * next time hits it's messaging server.
     *
     * Implementation notes: This shouldn't need to be used unless we're certain. When we introduce
     * [a polling service][0] to check if endpoints are expired, we would invoke this.
     *
     * [0]: https://github.com/mozilla-mobile/android-components/issues/3173
     */
    fun forceRegistrationRenewal() {
        // Remove the cached token we have.
        deleteToken(context)

        // Tell the service to delete the token as well, which will trigger a new token to be
        // retrieved the next time it hits the server.
        service.deleteToken()
    }

    private fun CoroutineScope.launchAndTry(block: suspend CoroutineScope.() -> Unit) {
        job = launch {
            try {
                block()
            } catch (e: RustPushError) {
                onError(PushError.Rust(e.toString()))
            }
        }
    }

    private fun saveToken(context: Context, value: String) {
        preferences(context).edit().putString(PREF_TOKEN, value).apply()
    }

    private fun deleteToken(context: Context) {
        preferences(context).edit().remove(PREF_TOKEN).apply()
    }

    private fun preferences(context: Context): SharedPreferences =
        context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)

    companion object {
        internal const val PREFERENCE_NAME = "mozac_feature_push"
        internal const val PREF_TOKEN = "token"
        internal const val DB_NAME = "push.sqlite"
    }
}

/**
 * The different kind of message types that a [EncryptedPushMessage] can be:
 *  - Application Services (e.g. FxA/Send Tab)
 *  - WebPush messages (see: https://www.w3.org/TR/push-api/)
 */
enum class PushType {
    Services,
    WebPush;

    /**
     * Retrieve a channel ID from the PushType.
     *
     * This is a reproducible UUID that is used for mapping a push message type with the push manager.
     */
    fun toChannelId() = UUID.nameUUIDFromBytes(name.toByteArray()).toString().replace("-", "")
}

/**
 * Observers that want to receive updates for new subscriptions.
 */
interface PushSubscriptionObserver {
    fun onSubscriptionAvailable(subscription: AutoPushSubscription)
}

/**
 * This is manager mapping of service type to channel ID, it will eventually be replaced by the
 * Application Service implementation.
 */
internal object DeliveryManager {
    fun serviceForChannelId(channelId: String): PushType {
        return PushType.values().first { it.toChannelId() == channelId }
    }

    /**
     * Executes the block if the Push Manager is initialized.
     */
    fun with(connection: PushConnection, block: PushConnection.() -> Unit) {
        if (connection.isInitialized()) {
            block(connection)
        }
    }
}

/**
 * Supported push services. These are currently limited to Firebase Cloud Messaging and Amazon Device Messaging.
 */
enum class ServiceType {
    FCM,
    ADM
}

/**
 * Supported network protocols.
 */
enum class Protocol {
    HTTP,
    HTTPS
}

/**
 * The subscription information from AutoPush that can be used to send push messages to other devices.
 */
data class AutoPushSubscription(
    val type: PushType,
    val endpoint: String,
    val publicKey: String,
    val authKey: String
)

/**
 * Configuration object for initializing the Push Manager with an AutoPush server.
 *
 * @param senderId The project identifier set by the server. Contact your server ops team to know what value to set.
 * @param serverHost The sync server address.
 * @param protocol The socket protocol to use when communicating with the server.
 * @param serviceType The push services that the AutoPush server supports.
 */
data class PushConfig(
    val senderId: String,
    val serverHost: String = "updates.push.services.mozilla.com",
    val protocol: Protocol = Protocol.HTTPS,
    val serviceType: ServiceType = ServiceType.FCM
)

/**
 * A helper to convert the internal data class.
 */
internal fun SubscriptionResponse.toPushSubscription(): AutoPushSubscription {
    val type = DeliveryManager.serviceForChannelId(channelID)
    return AutoPushSubscription(
        type = type,
        endpoint = subscriptionInfo.endpoint,
        authKey = subscriptionInfo.keys.auth,
        publicKey = subscriptionInfo.keys.p256dh
    )
}
