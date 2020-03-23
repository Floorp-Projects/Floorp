/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.concept.sync.AccountObserver as SyncAccountObserver
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.SharedPreferencesCache
import org.json.JSONObject
import java.util.UUID

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
 * @param owner the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner].
 * @param autoPause whether to stop notifying the observer during onPause lifecycle events.
 * Defaults to false so that observers are always notified.
 */
class FxaPushSupportFeature(
    private val context: Context,
    accountManager: FxaAccountManager,
    pushFeature: AutoPushFeature,
    owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    autoPause: Boolean = false
) {

    /**
     * A unique scope for the FxA push subscription that is generated once and stored in SharedPreferences.
     *
     * This scope is randomly generated and unique to the account on that particular device.
     */
    private val fxaPushScope: String by lazy {
        val prefs = preference(context)

        // Generate a unique scope if one doesn't exist.
        val randomUuid = UUID.randomUUID().toString().replace("-", "")

        // Return a scope in the format example: "fxa_push_scope_a62d5f27c9d74af4996d057f0e0e9c38"
        val scope = PUSH_SCOPE_PREFIX + randomUuid

        if (!prefs.contains(PREF_FXA_SCOPE)) {
            prefs.edit().putString(PREF_FXA_SCOPE, scope).apply()

            return@lazy scope
        }

        // The default string is non-null, so we can safely cast.
        prefs.getString(PREF_FXA_SCOPE, scope) as String
    }

    init {
        val autoPushObserver = AutoPushObserver(accountManager, fxaPushScope)

        val accountObserver = AccountObserver(
            context,
            pushFeature,
            fxaPushScope,
            owner,
            autoPause
        )

        accountManager.register(accountObserver)

        pushFeature.register(autoPushObserver, owner, autoPause)
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
    private val lifecycleOwner: LifecycleOwner,
    private val autoPause: Boolean
) : SyncAccountObserver {

    private val logger = Logger(AccountObserver::class.java.simpleName)
    private val constellationObserver = ConstellationObserver(context, push)
    private val pushReset = OneTimeFxaPushReset(context, push)

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {

        pushReset.resetSubscriptionIfNeeded(account)

        // We need a new subscription only when we have a new account.
        // The subscription is removed when an account logs out.
        if (authType != AuthType.Existing && authType != AuthType.Recovered) {
            logger.debug("Subscribing for FxaPushScope ($fxaPushScope) events.")

            push.subscribe(fxaPushScope) { subscription ->
                account.deviceConstellation().setDevicePushSubscriptionAsync(
                    DevicePushSubscription(
                        endpoint = subscription.endpoint,
                        publicKey = subscription.publicKey,
                        authKey = subscription.authKey
                    )
                )
            }
        }

        account.deviceConstellation().registerDeviceObserver(constellationObserver, lifecycleOwner, autoPause)
    }

    override fun onLoggedOut() {
        logger.debug("Un-subscribing for FxA scope $fxaPushScope events.")

        push.unsubscribe(fxaPushScope)

        // Delete cached value of last verified timestamp and scope when we log out.
        preference(context).edit()
            .remove(PREF_LAST_VERIFIED)
            .apply()
    }
}

/**
 * A [DeviceConstellation] observer to know when we should notify the push feature to begin the registration renewal
 * when notified by the FxA server. See [Device.subscriptionExpired].
 */
internal class ConstellationObserver(
    context: Context,
    private val push: PushProcessor,
    private val verifier: VerificationDelegate = VerificationDelegate(context)
) : DeviceConstellationObserver {

    private val logger = Logger(ConstellationObserver::class.java.simpleName)

    override fun onDevicesUpdate(constellation: ConstellationState) {
        val updateSubscription = constellation.currentDevice?.subscriptionExpired ?: false

        // If our subscription has not expired, we do nothing.
        // If our last check was recent (see: PERIODIC_INTERVAL_MILLISECONDS), we do nothing.
        if (!updateSubscription || !verifier.allowedToRenew()) {
            return
        }

        logger.warn("We have been notified that our push subscription has expired; renewing registration.")

        push.renewRegistration()

        verifier.increment()
    }
}

/**
 * An [AutoPushFeature] observer to handle [FxaAccountManager] subscriptions and push events.
 */
internal class AutoPushObserver(
    private val accountManager: FxaAccountManager,
    private val fxaPushScope: String
) : AutoPushFeature.Observer {
    private val logger = Logger(AutoPushObserver::class.java.simpleName)

    override fun onMessageReceived(scope: String, message: ByteArray?) {
        logger.info("Received new push message for $scope")

        // Ignore messages that are not meant for us.
        if (scope != fxaPushScope) {
            return
        }

        // Ignore push messages that do not have data.
        val rawEvent = message ?: return

        accountManager.withConstellation {
            it.processRawEventAsync(String(rawEvent))
        }
    }
}

/**
 * A helper that rate limits how often we should notify our servers to renew push registration.
 *
 * Implementation notes: This saves the timestamp of our renewal and the number of times we have renewed our
 * registration within the [PERIODIC_INTERVAL_MILLISECONDS] interval of time.
 */
internal class VerificationDelegate(context: Context) : SharedPreferencesCache<VerificationState>(context) {
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
            obj.getInt(KEY_TOTAL_COUNT)
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
        val withinTimeFrame = System.currentTimeMillis() - innerTimestamp < PERIODIC_INTERVAL_MILLISECONDS
        val withinIntervalCounter = innerCount <= MAX_REQUEST_IN_INTERVAL
        val shouldAllow = withinTimeFrame && withinIntervalCounter

        // If it's been PERIODIC_INTERVAL_MILLISECONDS since we last checked, we can reset
        // out rate limiter and verify now.
        if (!withinTimeFrame) {
            reset()
            return true
        }

        return shouldAllow
    }

    /**
     * Should be called whenever a successful invocation has taken place and we want to record it.
     */
    fun increment() {
        val count = innerCount + 1

        setToCache(VerificationState(innerTimestamp, count))

        innerCount = count
    }

    private fun reset() {
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

/**
 * Resets the fxa push scope (and therefore push subscription) if it does not follow the new format.
 *
 * This is needed only for our existing push users and can be removed when we're more confident our users are
 * all migrated.
 *
 * Implementation Notes: In order to support a new performance fix related to push and
 * [FxaAccountManager] we need to use a new push scope format. This class checks if we have the old
 * format, and removes it if so, thereby generating a new push scope with the new format.
 */
class OneTimeFxaPushReset(
    private val context: Context,
    private val pushFeature: AutoPushFeature
) {

    /**
     * Resets the push subscription if the old subscription format is used.
     */
    fun resetSubscriptionIfNeeded(account: OAuthAccount) {
        val pushScope = preference(context).getString(PREF_FXA_SCOPE, null) ?: return

        if (pushScope.contains(FxaPushSupportFeature.PUSH_SCOPE_PREFIX)) {
            return
        }

        val newPushScope = FxaPushSupportFeature.PUSH_SCOPE_PREFIX + pushScope

        pushFeature.unsubscribe(pushScope)
        pushFeature.subscribe(newPushScope) { subscription ->
            account.deviceConstellation().setDevicePushSubscriptionAsync(
                DevicePushSubscription(
                    endpoint = subscription.endpoint,
                    publicKey = subscription.publicKey,
                    authKey = subscription.authKey
                )
            )
        }

        preference(context).edit().putString(PREF_FXA_SCOPE, newPushScope).apply()
    }
}

internal data class VerificationState(val timestamp: Long, val totalCount: Int)

@VisibleForTesting
internal fun preference(context: Context) = context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
