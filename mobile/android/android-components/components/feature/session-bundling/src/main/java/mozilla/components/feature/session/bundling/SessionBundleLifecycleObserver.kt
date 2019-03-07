/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.TimeUnit

/**
 * [LifecycleObserver] implementation that will close an active session bundle if its lifetime has expired.
 *
 * Usually the app doesn't restore a bundle that is not active. However the app process may continue to life for an
 * unspecified amount of time if the app is in the background. The next time the user switches back to the app (after
 * the lifetime has expired) the previous state may still be in memory. This [LifecycleObserver] will clear the bundle
 * from [SessionBundleStorage] (so that a new bundle will be used) and removes the associated sessions from
 * [SessionManager].
 */
internal class SessionBundleLifecycleObserver(
    private val storage: SessionBundleStorage,
    private val sessionManager: SessionManager
) : LifecycleObserver {
    private val logger = Logger("SessionBundleLifecycleObserver")
    @VisibleForTesting
    internal var backgroundJob: Job? = null
    @VisibleForTesting internal var mainDispatcher: CoroutineDispatcher = Dispatchers.Main

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onStart() {
        backgroundJob?.cancel()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onStop() {
        backgroundJob = GlobalScope.launch(Dispatchers.IO) {
            val delayMilliseconds = TimeUnit.MILLISECONDS.convert(
                storage.bundleLifetime.first,
                storage.bundleLifetime.second)

            logger.debug("Lifetime job started: ${delayMilliseconds}ms")

            try {
                delay(delayMilliseconds)
            } catch (e: CancellationException) {
                logger.debug("Lifetime job cancelled")
                throw e
            }

            withContext(mainDispatcher) {
                closeBundle()
            }
        }
    }

    private fun closeBundle() {
        logger.debug("Closing active bundle")

        // This does not protect against races yet:
        //
        // - AutoSave could be saving the state concurrently and then we may end up with a new bundle that contains the
        //   old state of the bundle we just closed. It is unlikely that this happens though: Most AutoSave triggers
        //   pause while the app is in the background. Only the "session changes" trigger remains active. We could
        //   consider pausing this trigger too while the app is in the background. However background changes like
        //   "website loaded" is something that we would want to persist.
        //
        // - Another potential race condition can come from session modifications that happen very early when the app
        //   is resumed (e.g. incoming intents). From this code it's not clear which part gets executed first: The
        //   intent processor that receives the intent and creates the new session or this code here that clears the
        //   session manager? Based on the order the new session could be in the closed bundle (boo) or in the new
        //   bundle (yay!)
        //
        // TL;DR: We need some way to synchronize this part of the code with "AutoSave" and the Intent Processor. So
        //        that racing changes to end up in the right bundle.

        storage.new()
        sessionManager.removeSessions()
    }
}
