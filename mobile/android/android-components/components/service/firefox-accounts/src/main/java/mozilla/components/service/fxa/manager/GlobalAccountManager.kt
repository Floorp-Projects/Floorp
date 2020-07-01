/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import androidx.annotation.VisibleForTesting
import java.lang.ref.WeakReference
import java.util.concurrent.TimeUnit

/**
 * A singleton which exposes an instance of [FxaAccountManager] for internal consumption.
 * Populated during initialization of [FxaAccountManager].
 * This exists to allow various internal parts without a direct reference to an instance of
 * [FxaAccountManager] to notify it of encountered auth errors via [authError].
 */
internal object GlobalAccountManager {
    private var instance: WeakReference<FxaAccountManager>? = null
    private var lastAuthErrorCheckPoint: Long = 0L
    private var authErrorCountWithinWindow: Int = 0

    internal interface Clock {
        fun getTimeCheckPoint(): Long
    }

    private val systemClock = object : Clock {
        override fun getTimeCheckPoint(): Long {
            // nanoTime to decouple from wall-time.
            return TimeUnit.NANOSECONDS.toMillis(System.nanoTime())
        }
    }

    internal fun setInstance(am: FxaAccountManager) {
        instance = WeakReference(am)
        lastAuthErrorCheckPoint = 0
        authErrorCountWithinWindow = 0
    }

    internal fun close() {
        instance = null
    }

    internal suspend fun authError(operation: String, @VisibleForTesting clock: Clock = systemClock) {
        val authErrorCheckPoint: Long = clock.getTimeCheckPoint()

        val timeSinceLastAuthErrorMs: Long? = if (lastAuthErrorCheckPoint == 0L) {
            null
        } else {
            authErrorCheckPoint - lastAuthErrorCheckPoint
        }
        lastAuthErrorCheckPoint = authErrorCheckPoint

        if (timeSinceLastAuthErrorMs == null) {
            // First error, start our count.
            authErrorCountWithinWindow = 1
        } else if (timeSinceLastAuthErrorMs <= AUTH_CHECK_CIRCUIT_BREAKER_RESET_MS) {
            // Error within the reset time window, increment the count.
            authErrorCountWithinWindow += 1
        } else {
            // Error outside the reset window, reset the count.
            authErrorCountWithinWindow = 1
        }

        instance?.get()?.encounteredAuthError(operation, authErrorCountWithinWindow)
    }
}
