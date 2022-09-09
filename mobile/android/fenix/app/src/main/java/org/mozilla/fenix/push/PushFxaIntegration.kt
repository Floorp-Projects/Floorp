/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.accounts.push.FxaPushSupportFeature
import mozilla.components.feature.accounts.push.SendTabFeature
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.PushScope
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import org.mozilla.fenix.components.BackgroundServices
import org.mozilla.fenix.components.Push

/**
 * A lazy initializer for FxaAccountManager if it isn't already initialized.
 *
 * Implementation notes: For push notifications, we need to initialize the service on
 * Application#onCreate as soon as possible in order to receive messages. These are then decrypted
 * and the observers of the push feature are notified.
 *
 * One of our observers is [FxaAccountManager] that needs to know about messages like Send Tab,
 * new account logins, etc. This however comes at the cost of having the account manager
 * initialized and observing the push feature when it initializes (which once again happens on
 * application create) - the total cost of startup time now is additive for the both of them.
 *
 * What this integration class aims to do, is to observe the push feature immediately in order to act
 * as a (temporary) delegate, and when we see a push message from FxA, only then we should
 * initialize and deliver the message.
 *
 * Once FxaAccountManager is initialized, we no longer need this integration as there already are
 * existing features to support these feature requirements, so we safely unregister ourselves.
 * See: [FxaPushSupportFeature] and [SendTabFeature].
 *
 * A solution that we considered was to pass in [BackgroundServices] to the [Push] class
 * and lazily invoke the account manager - that lead to a cyclic dependency of initialization since
 * [BackgroundServices] also depends on [Push] directly for observing messages via the account-based
 * features.
 *
 * Another solution was to create a message buffer to queue up the messages until the account could
 * consume them - this added the complexity of maintaining a buffer, the possibility of flooding the
 * buffer, and delaying the delivery of high importance messages like Send Tab which are required to
 * be processed immediately.
 *
 * Our final solution ended up being more concise that the above options that met all our required
 * assurances, and most importantly, maintainable.
 */
class PushFxaIntegration(
    private val pushFeature: AutoPushFeature,
    lazyAccountManager: Lazy<FxaAccountManager>,
) {
    private val observer =
        OneTimePushMessageObserver(
            lazyAccountManager,
            pushFeature,
        )

    /**
     * Starts the observer.
     *
     * This should be done before or as soon as push is initialized.
     */
    fun launch() {
        pushFeature.register(observer)
    }
}

/**
 * Observes push messages from [AutoPushFeature], then initializes [FxaAccountManager] if it isn't
 * already.
 */
internal class OneTimePushMessageObserver(
    private val lazyAccountManager: Lazy<FxaAccountManager>,
    private val pushFeature: AutoPushFeature,
) : AutoPushFeature.Observer {
    override fun onMessageReceived(scope: PushScope, message: ByteArray?) {
        // Ignore empty push messages.
        val rawBytes = message ?: return

        // If the push scope has the FxA prefix, we know this is for us.
        if (scope.contains(FxaPushSupportFeature.PUSH_SCOPE_PREFIX)) {
            // If we aren't initialized, then we should do the initialization and message delivery.
            if (!lazyAccountManager.isInitialized()) {
                CoroutineScope(Dispatchers.Main).launch {
                    val fxaObserver = OneTimeMessageDeliveryObserver(lazyAccountManager, rawBytes)

                    // Start observing the account manager, so that we can deliver our message
                    // only when we are authenticated and are capable of processing it.
                    lazyAccountManager.value.register(fxaObserver)
                }
            }

            MainScope().launch {
                // Remove ourselves when we're done.
                pushFeature.unregister(this@OneTimePushMessageObserver)
            }
        }
    }
}

/**
 * Waits for the [FxaAccountManager] to authenticate itself in order to deliver the [message], then
 * unregisters itself once complete.
 */
internal class OneTimeMessageDeliveryObserver(
    private val lazyAccount: Lazy<FxaAccountManager>,
    private val message: ByteArray,
) : AccountObserver {
    override fun onAuthenticated(
        account: OAuthAccount,
        authType: AuthType,
    ) {
        lazyAccount.value.withConstellation {
            MainScope().launch { processRawEvent(String(message)) }
        }

        MainScope().launch {
            lazyAccount.value.unregister(this@OneTimeMessageDeliveryObserver)
        }
    }
}
