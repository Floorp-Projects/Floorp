/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry.eventprocessors

import io.sentry.EventProcessor
import io.sentry.Hint
import io.sentry.SentryEvent
import mozilla.components.concept.base.crash.RustCrashReport as RustCrashReport

/**
 * A [EventProcessor] implementation that cleans up exceptions for
 * crashes coming from our Rust libraries.
 */
class RustCrashEventProcessor : EventProcessor {
    override fun process(event: SentryEvent, hint: Hint): SentryEvent {
        val throwable = event.throwable

        if (throwable is RustCrashReport) {
            event.fingerprints = listOf(throwable.typeName)
            // Sentry supports multiple exceptions in an event, modify
            // the top-level one controls how the event is displayed
            //
            // It's technically possible for the event to have a null
            // or empty exception list, but that shouldn't happen in
            // practice.
            event.exceptions?.firstOrNull()?.let { sentryException ->
                sentryException.type = throwable.typeName
                sentryException.value = throwable.message
            }
        }

        return event
    }
}
