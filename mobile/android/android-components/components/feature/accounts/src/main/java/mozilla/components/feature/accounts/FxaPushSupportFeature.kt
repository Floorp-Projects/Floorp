/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.AccountObserver as SyncAccountObserver
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.SharedPreferencesCache
import org.json.JSONObject

internal const val PREFERENCE_NAME = "mozac_feature_accounts"
internal const val LAST_VERIFIED = "last_verified_push_subscription"

/**
 * A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies
 * the device during a sync, that it's unable to reach the device via push messaging; triggering a push
 * registration renewal.
 *
 * @param context The application Android context.
 * @param accountManager The FxaAccountManager.
 * @param push The push processor instance that needs to be notified.
 * @param owner the lifecycle owner for the observer. Defaults to [ProcessLifecycleOwner].
 * @param autoPause whether to stop notifying the observer during onPause lifecycle events.
 * Defaults to false so that observers are always notified.
 */
class FxaPushSupportFeature(
    context: Context,
    accountManager: FxaAccountManager,
    push: PushProcessor,
    owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    autoPause: Boolean = false
) {
    init {
        val constellationObserver = ConstellationObserver(context, push)

        val accountObserver = AccountObserver(context, accountManager, constellationObserver, owner, autoPause)

        accountManager.register(accountObserver)
    }
}

/**
 * An [FxaAccountManager] observer to know when an account has been added, so we can begin observing the device
 * constellation.
 */
internal class AccountObserver(
    private val context: Context,
    private val accountManager: FxaAccountManager,
    private val observer: DeviceConstellationObserver,
    private val lifecycleOwner: LifecycleOwner,
    private val autoPause: Boolean
) : SyncAccountObserver {
    private val logger = Logger("AccountObserver")

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        accountManager.withConstellation { constellation ->
            constellation.registerDeviceObserver(observer, lifecycleOwner, autoPause)
        }
    }

    override fun onLoggedOut() {
        // Delete renewal pref.
        preference(context).edit().remove(LAST_VERIFIED).apply()
    }
}

/**
 * A DeviceConstellation observer to know when we should notify the push feature to begin the registration renewal.
 */
internal class ConstellationObserver(
    context: Context,
    private val push: PushProcessor,
    private val verifier: VerificationDelegate = VerificationDelegate(context)
) : DeviceConstellationObserver {

    private val logger = Logger("ConstellationObserver")

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
 * A helper that rate limits how often we should notify our servers to renew push registration.
 *
 * Implementation notes: This saves the timestamp of our renewal and the number of times we have renewed our
 * registration within the [PERIODIC_INTERVAL_MILLISECONDS] interval of time.
 */
internal class VerificationDelegate(context: Context) : SharedPreferencesCache<VerificationState>(context) {
    override val logger: Logger = Logger("VerificationDelegate")
    override val cacheKey: String = LAST_VERIFIED
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

internal data class VerificationState(val timestamp: Long, val totalCount: Int)

@VisibleForTesting
internal fun preference(context: Context) = context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
