/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.os.SystemClock
import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger
import java.lang.RuntimeException

private val logger = Logger("Clock")

/**
 * A wrapper around [SystemClock] and other time-related APIs. Allows mocking the underlying
 * behavior in tests.
 */
object Clock {
    @VisibleForTesting
    var delegate: Delegate = createDefaultDelegate()

    /**
     * Returns milliseconds since boot, including time spent in sleep.
     */
    fun elapsedRealtime(): Long = delegate.elapsedRealtime()

    /**
     * Interface for actual clock implementations that [Clock] will delegate to.
     */
    interface Delegate {
        /**
         * Returns milliseconds since boot, including time spent in sleep.
         */
        fun elapsedRealtime(): Long
    }

    @VisibleForTesting
    fun reset() {
        delegate = createDefaultDelegate()
    }
}

@Suppress("TooGenericExceptionCaught")
private fun createDefaultDelegate(): Clock.Delegate {
    // If android.os.SystemClock is available we will delegate `Clock` calls to it. Otherwise we
    // fallback to using `DummyClockDelegate`. This allows us to not have to mock anything in unit
    // tests that run on a JVM without Android stdlib available. If needed, tests can replace the
    // whole delegate with a custom (mock) implementation.
    return try {
        Class.forName("android.os.SystemClock")
        SystemClock.elapsedRealtime()
        AndroidClockDelegate()
    } catch (e: ClassNotFoundException) {
        // If android.os.SystemClock is not available then use the dummy clock.
        logger.info("android.os.SystemClock not available, using DummyClockDelegate")
        DummyClockDelegate()
    } catch (e: RuntimeException) {
        logger.info("SystemClock throws RuntimeException, using DummyClockDelegate")
        // If calling elapsedRealtime on SystemClock throws a RuntimeException (as done by the
        // android.jar file loaded in unit tests) then use the dummy clock.
        DummyClockDelegate()
    }
}

private class AndroidClockDelegate : Clock.Delegate {
    override fun elapsedRealtime() = SystemClock.elapsedRealtime()
}

private class DummyClockDelegate : Clock.Delegate {
    override fun elapsedRealtime(): Long = 0
}
