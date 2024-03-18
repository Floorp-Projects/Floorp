/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry.eventprocessors

import androidx.annotation.VisibleForTesting
import io.sentry.EventProcessor
import io.sentry.Hint
import io.sentry.SentryEvent
import io.sentry.SentryLevel
import io.sentry.protocol.Mechanism

/**
 * A [EventProcessor] implementation that adds a [Machanism]
 * to [SentryLevel.FATAL] events.
 */
class AddMechanismEventProcessor : EventProcessor {
    override fun process(event: SentryEvent, hint: Hint): SentryEvent {
        if (event.level == SentryLevel.FATAL) {
            // Sentry now uses the `Mechanism` to determine whether or not an exception is
            // handled. Any exception sent with `Sentry.captureException` is assumed to be handled
            // by Sentry. We can attach a `UncaughtExceptionHandler` mechanism to the `SentryException`
            // to correctly signal to Sentry that this is an uncaught exception.
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1835107
            event.exceptions?.firstOrNull()?.let { sentryException ->
                sentryException.mechanism = Mechanism().apply {
                    type = UNCAUGHT_EXCEPTION_TYPE
                    isHandled = false
                }
            }
        }

        return event
    }

    companion object {
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val UNCAUGHT_EXCEPTION_TYPE = "UncaughtExceptionHandler"
    }
}
