/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("LongParameterList")

package mozilla.components.feature.accounts.push

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.accounts.push.cache.PushScopeProperty
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.feature.push.PushScope
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.SharedPreferencesCache
import org.json.JSONObject
import mozilla.components.concept.sync.AccountObserver as SyncAccountObserver

internal const val PREFERENCE_NAME = "mozac_feature_accounts_push"
internal const val PREF_LAST_VERIFIED = "last_verified_push_subscription"
internal const val PREF_FXA_SCOPE = "fxa_push_scope"

/**
 * A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies
 * the device during a sync, that it's unable to reach the device via push messaging; triggering a push
 * registration renewal.
 *
 * @param context The application Android context.
 * @param accountManager The FxaAccountManager.
 * @param pushFeature The [AutoPushFeature] if that is setup for observing push events.
 * @param crashReporter Instance of `CrashReporting` to record unexpected caught exceptions.
 * @param coroutineScope The scope in which IO work within the feature should be performed on.
 * @param owner the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner].
 * @param autoPause whether to stop notifying the observer during onPause lifecycle events.
 * Defaults to false so that observers are always notified.
 */
class FxaPushSupportFeature(
    private val context: Context,
    private val accountManager: FxaAccountManager,
    private val pushFeature: AutoPushFeature,
    private val crashReporter: CrashReporting? = null,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    private val autoPause: Boolean = false,
) {

    /**
     * A unique scope for the FxA push subscription that is generated once and stored in SharedPreferences.
     *
     * This scope is randomly generated and unique to the app install.
     * (why this uuid? Note it is *not* reset on logout!)
     */
    private val pushScope = PushScopeProperty(context, coroutineScope)

    /**
     * Initialize the support feature to launch the appropriate observers.
     */
    fun initialize() = coroutineScope.launch {
        val scopeValue = pushScope.value()

        val autoPushObserver = AutoPushObserver(accountManager, pushFeature, scopeValue)

        val accountObserver = AccountObserver(
            context,
            pushFeature,
            scopeValue,
            crashReporter,
            owner,
            autoPause,
        )

        coroutineScope.launch(Main) {
            accountManager.register(accountObserver)

            pushFeature.register(autoPushObserver, owner, autoPause)
        }
    }

    companion object {
        const val PUSH_SCOPE_PREFIX = "fxa_push_scope_"
    }
}

/**
 * An [FxaAccountManager] observer to know when an account has been added, so we can begin observing the device
 * constellation.
 */
internal class AccountObserver(
    private val context: Context,
    private val push: AutoPushFeature,
    private val fxaPushScope: String,
    private val crashReporter: CrashReporting?,
    private val lifecycleOwner: LifecycleOwner,
    private val autoPause: Boolean,
) : SyncAccountObserver {

    private val logger = Logger(AccountObserver::class.java.simpleName)
    private val verificationDelegate = VerificationDelegate(context, push.config.disableRateLimit)

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        val constellationObserver = ConstellationObserver(
            context = context,
            push = push,
            scope = fxaPushScope,
            account = account,
            verifier = verificationDelegate,
            crashReporter = crashReporter,
        )

        // NB: can we just expose registerDeviceObserver on account manager?
        // registration could happen after onDevicesUpdate has been called, without having to tie this
        // into the account "auth lifecycle".
        // See https://github.com/mozilla-mobile/android-components/issues/8766
        GlobalScope.launch(Main) {
            account.deviceConstellation().registerDeviceObserver(constellationObserver, lifecycleOwner, autoPause)
            account.deviceConstellation().refreshDevices()
        }
    }

    override fun onLoggedOut() {
        logger.debug("Un-subscribing for FxA scope $fxaPushScope events.")

        push.unsubscribe(fxaPushScope)

        // Delete cached value of last verified timestamp when we log out.
        preference(context).edit()
            .remove(PREF_LAST_VERIFIED)
            .apply()
    }
}

/**
 * Subscribes to the AutoPushFeature, and updates the FxA device record if necessary.
 * Note that if the subscription already exists, then this doesn't hit any servers, so
 * it's OK to call this somewhat frequently.
 */
internal fun pushSubscribe(
    push: AutoPushFeature,
    account: OAuthAccount,
    scope: String,
    crashReporter: CrashReporting?,
    logContext: String,
) {
    val logger = Logger("FxaPushSupportFeature")
    val currentDevice = account.deviceConstellation().state()?.currentDevice
    if (currentDevice == null) {
        logger.warn("Can't subscribe to account push notifications as there's no current device")
        return
    }
    logger.debug("Subscribing for FxaPushScope ($scope) events.")
    push.subscribe(
        scope,
        onSubscribeError = { e ->
            crashReporter?.recordCrashBreadcrumb(Breadcrumb("Subscribing to FxA push failed."))
            logger.warn("Subscribing to FxA push failed: $logContext: ", e)
        },
        onSubscribe = { subscription ->
            logger.info("Created a new subscription: $logContext: $subscription")
            // Apparently `subscriptionExpired` typically means just the FCM token is wrong, so
            // after getting a new one, our push endpoint will remain the same as it was. So here
            // we always update the endpoint if `subscriptionExpired` is true, even when the
            // subscription matches, just to ensure `subscriptionExpired` is reset.
            if (currentDevice.subscriptionExpired ||
                currentDevice.subscription?.endpoint != subscription.endpoint
            ) {
                logger.info("Updating account with new subscription info.")
                CoroutineScope(Main).launch {
                    account.deviceConstellation().setDevicePushSubscription(subscription.into())
                }
            }
        },
    )
}

/**
 * A [DeviceConstellation] observer to know when we should notify the push feature to begin the registration renewal
 * when notified by the FxA server. See [Device.subscriptionExpired].
 */
internal class ConstellationObserver(
    context: Context,
    private val push: AutoPushFeature,
    private val scope: String,
    private val account: OAuthAccount,
    private val verifier: VerificationDelegate = VerificationDelegate(context),
    private val crashReporter: CrashReporting?,
) : DeviceConstellationObserver {

    private val logger = Logger(ConstellationObserver::class.java.simpleName)

    override fun onDevicesUpdate(constellation: ConstellationState) {
        logger.info("onDevicesUpdate triggered.")

        val currentDevice = constellation.currentDevice ?: return
        if (currentDevice.subscriptionExpired) {
            if (verifier.allowedToRenew()) {
                // This smells wrong - the fact FxaPushSupportFeature needs to detect a
                // subscription problem implies non-FxA users will never detect this and
                // remain broken?
                logger.info("Our push subscription is expired; renewing FCM registration.")
                push.renewRegistration()

                logger.info("Incrementing verifier")
                logger.debug(
                    "Verifier state before: timestamp=${verifier.innerTimestamp}, count=${verifier.innerCount}",
                )
                verifier.increment()
                logger.debug(
                    "Verifier state after: timestamp=${verifier.innerTimestamp}, count=${verifier.innerCount}",
                )
            } else {
                logger.info("Short-circuiting onDevicesUpdate: rate-limited")
            }
        }

        // And unconditionally subscribe - if our local DB already has a subscription it will
        // be returned without hitting the server. If some other problem meant our subscription
        // was dropped or never made, it will hit the server and deliver a new end-point.
        pushSubscribe(push, account, scope, crashReporter, "onDevicesUpdate")
    }
}

/**
 * An [AutoPushFeature] observer to handle [FxaAccountManager] subscriptions and push events.
 */
internal class AutoPushObserver(
    private val accountManager: FxaAccountManager,
    private val pushFeature: AutoPushFeature,
    private val fxaPushScope: String,
) : AutoPushFeature.Observer {
    private val logger = Logger(AutoPushObserver::class.java.simpleName)

    override fun onMessageReceived(scope: String, message: ByteArray?) {
        if (scope != fxaPushScope) {
            return
        }

        logger.info("Received new push message for $scope")

        // Ignore push messages that do not have data.
        val rawEvent = message ?: return

        accountManager.withConstellation {
            CoroutineScope(Main).launch {
                processRawEvent(String(rawEvent))
            }
        }
    }

    override fun onSubscriptionChanged(scope: PushScope) {
        if (scope != fxaPushScope) {
            return
        }

        logger.info("Our sync push scope ($scope) has expired. Re-subscribing..")

        val account = accountManager.authenticatedAccount()
        if (account == null) {
            logger.info("We don't have any account to pass the push subscription to.")
            return
        }
        pushSubscribe(pushFeature, account, fxaPushScope, null, "onSubscriptionChanged")
    }
}

/**
 * A helper that rate limits how often we should notify our servers to renew push registration. For debugging, we
 * can override this rate-limit check by enabling the [disableRateLimit] flag.
 *
 * Implementation notes: This saves the timestamp of our renewal and the number of times we have renewed our
 * registration within the [PERIODIC_INTERVAL_MILLISECONDS] interval of time.
 */
internal class VerificationDelegate(
    context: Context,
    private val disableRateLimit: Boolean = false,
) : SharedPreferencesCache<VerificationState>(context) {
    override val logger: Logger = Logger(VerificationDelegate::class.java.simpleName)
    override val cacheKey: String = PREF_LAST_VERIFIED
    override val cacheName: String = PREFERENCE_NAME

    override fun VerificationState.toJSON() =
        JSONObject().apply {
            put(KEY_TIMESTAMP, timestamp)
            put(KEY_TOTAL_COUNT, totalCount)
        }

    override fun fromJSON(obj: JSONObject) =
        VerificationState(
            obj.getLong(KEY_TIMESTAMP),
            obj.getInt(KEY_TOTAL_COUNT),
        )

    @VisibleForTesting
    internal var innerCount: Int = 0

    @VisibleForTesting
    internal var innerTimestamp: Long = System.currentTimeMillis()

    init {
        getCached()?.let { cache ->
            innerTimestamp = cache.timestamp
            innerCount = cache.totalCount
        }
    }

    /**
     * Checks whether we're within our rate limiting constraints.
     */
    fun allowedToRenew(): Boolean {
        logger.info("Allowed to renew?")

        if (disableRateLimit) {
            logger.info("Rate limit override is enabled - allowed to renew!")
            return true
        }

        // within time frame
        val currentTime = System.currentTimeMillis()
        if ((currentTime - innerTimestamp) >= PERIODIC_INTERVAL_MILLISECONDS) {
            logger.info("Resetting. currentTime($currentTime) - $innerTimestamp < $PERIODIC_INTERVAL_MILLISECONDS")
            reset()
        } else {
            logger.info("No need to reset inner timestamp and count.")
        }

        // within interval counter
        if (innerCount > MAX_REQUEST_IN_INTERVAL) {
            logger.info("Not allowed: innerCount($innerCount) > $MAX_REQUEST_IN_INTERVAL")
            return false
        }

        logger.info("Allowed to renew!")
        return true
    }

    /**
     * Should be called whenever a successful invocation has taken place and we want to record it.
     */
    fun increment() {
        logger.info("Incrementing verification state.")
        val count = innerCount + 1

        setToCache(VerificationState(innerTimestamp, count))

        innerCount = count
    }

    private fun reset() {
        logger.info("Resetting verification state.")
        val timestamp = System.currentTimeMillis()
        innerCount = 0
        innerTimestamp = timestamp

        setToCache(VerificationState(timestamp, 0))
    }

    companion object {
        private const val KEY_TIMESTAMP = "timestamp"
        private const val KEY_TOTAL_COUNT = "totalCount"

        internal const val PERIODIC_INTERVAL_MILLISECONDS = 24 * 60 * 60 * 1000L // 24 hours
        internal const val MAX_REQUEST_IN_INTERVAL = 500 // 500 requests in 24 hours
    }
}

internal data class VerificationState(val timestamp: Long, val totalCount: Int)

internal fun preference(context: Context) = context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)

internal fun AutoPushSubscription.into() = DevicePushSubscription(
    endpoint = this.endpoint,
    publicKey = this.publicKey,
    authKey = this.authKey,
)
