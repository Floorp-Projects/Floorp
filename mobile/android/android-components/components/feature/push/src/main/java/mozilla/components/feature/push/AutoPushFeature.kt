/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import mozilla.appservices.push.PushException.CommunicationException
import mozilla.appservices.push.PushException.CommunicationServerException
import mozilla.appservices.push.PushException.CryptoException
import mozilla.appservices.push.PushException.GeneralException
import mozilla.appservices.push.PushException.JsonDeserializeException
import mozilla.appservices.push.PushException.RequestException
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.push.PushService
import mozilla.components.feature.push.ext.ifInitialized
import mozilla.components.feature.push.ext.launchAndTry
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.base.utils.NamedThreadFactory
import java.util.concurrent.Executors
import kotlin.coroutines.CoroutineContext

/**
 * A implementation of a [PushProcessor] that should live as a singleton by being installed
 * in the Application's onCreate. It receives messages from a service and forwards them
 * to be decrypted and routed.
 *
 * ```kotlin
 * class Application {
 *      override fun onCreate() {
 *          val feature = AutoPushFeature(context, service, configuration)
 *          PushProvider.install(push)
 *      }
 * }
 * ```
 *
 * Observe for subscription information changes for each registered scope:
 *
 * ```kotlin
 * feature.register(object: AutoPushFeature.Observer {
 *     override fun onSubscriptionChanged(scope: PushScope) { }
 * })
 *
 * feature.subscribe("push_subscription_scope_id")
 * ```
 *
 * You should also observe for push messages:
 *
 * ```kotlin
 * feature.register(object: AutoPushFeature.Observer {
 *     override fun onMessageReceived(scope: PushScope, message: ByteArray?) { }
 * })
 * ```
 *
 * @param context the application [Context].
 * @param service A [PushService] bridge that receives the encrypted push messages - eg, Firebase.
 * @param config An instance of [PushConfig] to configure the feature.
 * @param coroutineContext An instance of [CoroutineContext] used for executing async push tasks.
 * @param connection An implementation of [PushConnection] to communicate with autopush - eg, app-services component.
 * @param crashReporter An optional instance of a [CrashReporting].
 */
@Suppress("LargeClass", "LongParameterList")
class AutoPushFeature(
    private val context: Context,
    private val service: PushService,
    val config: PushConfig,
    coroutineContext: CoroutineContext = Executors.newSingleThreadExecutor(
        NamedThreadFactory("AutoPushFeature"),
    ).asCoroutineDispatcher(),
    private val connection: PushConnection = RustPushConnection(
        context = context,
        senderId = config.senderId,
        serverHost = config.serverHost,
        socketProtocol = config.protocol,
        serviceType = config.serviceType,
    ),
    private val crashReporter: CrashReporting? = null,
) : PushProcessor, Observable<AutoPushFeature.Observer> by ObserverRegistry() {

    private val logger = Logger("AutoPushFeature")

    // The preference that stores new registration tokens.
    private val prefToken: String?
        get() = preferences(context).getString(PREF_TOKEN, null)
    private var prefLastVerified: Long
        get() = preferences(context).getLong(LAST_VERIFIED, 0)
        set(value) = preferences(context).edit().putLong(LAST_VERIFIED, value).apply()

    private val coroutineScope = CoroutineScope(coroutineContext) + SupervisorJob() + exceptionHandler { onError(it) }

    /**
     * Starts the push feature and initialization work needed. Also starts the [PushService] to ensure new messages
     * come through.
     */
    override fun initialize() {
        // If we have a token, initialize the rust component on a different thread.
        coroutineScope.launch {
            prefToken?.let { token ->
                logger.debug("Initializing rust component with the cached token.")
                connection.updateToken(token)
                tryVerifySubscriptions()
            }
        }
        // Starts the (FCM) push feature so that we receive messages if the service is not already started (safe call).
        service.start(context)
    }

    /**
     * Un-subscribes from all push message channels and stops periodic verifications.
     *
     * We do not stop the push service in case there are other consumers are using it as well. The app should
     * explicitly stop the service if desired.
     *
     * This should only be done on an account logout or app data deletion.
     */
    override fun shutdown() {
        connection.ifInitialized {
            coroutineScope.launch {
                unsubscribeAll()
            }
        }

        // Reset the push subscription check.
        prefLastVerified = 0L
    }

    /**
     * New registration tokens are received and sent to the AutoPush server which also performs subscriptions for
     * each push type and notifies the subscribers.
     */
    override fun onNewToken(newToken: String) {
        coroutineScope.launchAndTry {
            logger.info("Received a new registration token from push service.")

            saveToken(context, newToken)

            // Tell the autopush service about it and update subscriptions.
            connection.updateToken(newToken)
            tryVerifySubscriptions()
        }
    }

    /**
     * New encrypted messages received from a supported push messaging service.
     */
    override fun onMessageReceived(message: EncryptedPushMessage) {
        connection.ifInitialized {
            coroutineScope.launchAndTry {
                logger.info("New push message decrypted.")

                val (scope, decryptedMessage) = decryptMessage(
                    channelId = message.channelId,
                    body = message.body,
                    encoding = message.encoding,
                    salt = message.salt,
                    cryptoKey = message.cryptoKey,
                ) ?: return@launchAndTry

                notifyObservers { onMessageReceived(scope, decryptedMessage) }
            }
        }
    }

    override fun onError(error: PushError) {
        logger.error("${error.javaClass.simpleName} error: ${error.message}")

        crashReporter?.submitCaughtException(error)
    }

    /**
     * Subscribes for push notifications and invokes the [onSubscribe] callback with the subscription information.
     *
     * @param scope The subscription identifier which usually represents the website's URI.
     * @param appServerKey An optional key provided by the application server.
     * @param onSubscribeError The callback invoked with an [Exception] if the call does not successfully complete.
     * @param onSubscribe The callback invoked when a subscription for the [scope] is created.
     */
    fun subscribe(
        scope: String,
        appServerKey: String? = null,
        onSubscribeError: (Exception) -> Unit = {},
        onSubscribe: ((AutoPushSubscription) -> Unit) = {},
    ) {
        connection.ifInitialized {
            coroutineScope.launchAndTry(
                errorBlock = { exception ->
                    onSubscribeError(exception)
                },
                block = {
                    val sub = subscribe(scope, appServerKey)
                    onSubscribe(sub)
                },
            )
        }
    }

    /**
     * Un-subscribes from a valid subscription and invokes the [onUnsubscribe] callback with the result.
     *
     * @param scope The subscription identifier which usually represents the website's URI.
     * @param onUnsubscribeError The callback invoked with an [Exception] if the call does not successfully complete.
     * @param onUnsubscribe The callback invoked when a subscription for the [scope] is removed.
     */
    fun unsubscribe(
        scope: String,
        onUnsubscribeError: (Exception) -> Unit = {},
        onUnsubscribe: (Boolean) -> Unit = {},
    ) {
        connection.ifInitialized {
            coroutineScope.launchAndTry(
                errorBlock = { exception ->
                    onUnsubscribeError(exception)
                },
                block = {
                    val result = unsubscribe(scope)

                    if (result) {
                        onUnsubscribe(result)
                    } else {
                        onUnsubscribeError(IllegalStateException("Un-subscribing with the native client failed."))
                    }
                },
            )
        }
    }

    /**
     * Checks if a subscription for the [scope] already exists.
     *
     * @param scope The subscription identifier which usually represents the website's URI.
     * @param appServerKey An optional key provided by the application server.
     * @param block The callback invoked when a subscription for the [scope] is found, otherwise null. Note: this will
     * not execute on the calls thread.
     */
    fun getSubscription(
        scope: String,
        appServerKey: String? = null,
        block: (AutoPushSubscription?) -> Unit,
    ) {
        connection.ifInitialized {
            coroutineScope.launchAndTry {
                if (containsSubscription(scope)) {
                    // If we have a subscription, calling subscribe will give us the existing subscription.
                    // We do this because we do not have API symmetry across the different layers in this stack.
                    subscribe(scope, appServerKey, {}, block)
                } else {
                    block(null)
                }
            }
        }
    }

    /**
     * Deletes the FCM registration token locally so that it forces the service to get a new one the
     * next time hits it's messaging server.
     * XXX - this is suspect - the only caller of this is FxA, and it calls it when the device
     * record indicates the end-point is expired. If that's truly necessary, then it will mean
     * push never recovers for non-FxA users. If that's not truly necessary, we should remove it!
     */
    override fun renewRegistration() {
        logger.warn("Forcing FCM registration renewal by deleting our (cached) token.")

        // Remove the cached token we have.
        deleteToken(context)

        // Tell the service to delete the token as well, which will trigger a new token to be
        // retrieved the next time it hits the server.
        service.deleteToken()

        // Starts the service if needed to trigger a new registration.
        service.start(context)
    }

    /**
     * Verifies status (active, expired) of the push subscriptions and then notifies observers.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal suspend fun verifyActiveSubscriptions() {
        connection.ifInitialized {
            val subscriptionChanges = verifyConnection()

            if (subscriptionChanges.isNotEmpty()) {
                logger.info("Subscriptions have changed; notifying observers..")

                subscriptionChanges.forEach { sub ->
                    notifyObservers { onSubscriptionChanged(sub.scope) }
                }
            } else {
                logger.info("No change to subscriptions. Doing nothing.")
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun tryVerifySubscriptions() = coroutineScope.launch {
        logger.info("Trying to check validity of push subscriptions.")

        if (config.disableRateLimit || shouldVerifyNow()) {
            logger.info("Checking now..")

            verifyActiveSubscriptions()

            prefLastVerified = System.currentTimeMillis()
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

    /**
     * We should verify if it's been [PERIODIC_INTERVAL_MILLISECONDS] since our last attempt (successful or not).
     */
    private fun shouldVerifyNow(): Boolean {
        return (System.currentTimeMillis() - prefLastVerified) >= PERIODIC_INTERVAL_MILLISECONDS
    }

    /**
     * Observers that want to receive updates for new subscriptions and messages.
     */
    interface Observer {

        /**
         * A subscription for the scope is available.
         */
        fun onSubscriptionChanged(scope: PushScope) = Unit

        /**
         * A messaged has been received for the [scope].
         */
        fun onMessageReceived(scope: PushScope, message: ByteArray?) = Unit
    }

    companion object {
        internal const val PREFERENCE_NAME = "mozac_feature_push"
        internal const val PREF_TOKEN = "token"

        internal const val LAST_VERIFIED = "last_verified_push_connection"
        internal const val PERIODIC_INTERVAL_MILLISECONDS = 24 * 60 * 60 * 1000L // 24 hours
    }
}

internal inline fun exceptionHandler(crossinline onError: (PushError) -> Unit) = CoroutineExceptionHandler { _, e ->
    val isFatal = when (e) {
        is PushError.MalformedMessage,
        is GeneralException,
        is CryptoException,
        is CommunicationException,
        is JsonDeserializeException,
        is RequestException,
        is CommunicationServerException,
        -> false
        else -> true
    }

    if (isFatal) {
        onError(PushError.Rust(e, e.message.orEmpty()))
    } else {
        Logger.warn("Non-fatal error occurred in AutoPushFeature.", e)
    }
}

/**
 * Supported push services. This are currently limited to Firebase Cloud Messaging and
 * (previously) Amazon Device Messaging.
 */
enum class ServiceType {
    FCM,
}

/**
 * Supported network protocols.
 */
enum class Protocol {
    HTTP,
    HTTPS,
}

/**
 * The subscription information from AutoPush that can be used to send push messages to other devices.
 */
data class AutoPushSubscription(
    val scope: PushScope,
    val endpoint: String,
    val publicKey: String,
    val authKey: String,
    val appServerKey: String?,
)

/**
 * The subscription from AutoPush that has changed on the remote push servers.
 */
data class AutoPushSubscriptionChanged(
    val scope: PushScope,
    val channelId: String,
)

/**
 * Configuration object for initializing the Push Manager with an AutoPush server.
 *
 * @param senderId The project identifier set by the server. Contact your server ops team to know what value to set.
 * @param serverHost The sync server address.
 * @param protocol The socket protocol to use when communicating with the server.
 * @param serviceType The push services that the AutoPush server supports.
 * @param disableRateLimit A flag to disable our rate-limit logic. This is useful when debugging.
 */
data class PushConfig(
    val senderId: String,
    val serverHost: String = "updates.push.services.mozilla.com",
    val protocol: Protocol = Protocol.HTTPS,
    val serviceType: ServiceType = ServiceType.FCM,
    val disableRateLimit: Boolean = false,
)
