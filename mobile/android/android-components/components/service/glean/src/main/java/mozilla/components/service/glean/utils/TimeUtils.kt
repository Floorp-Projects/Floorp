/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.utils

import mozilla.components.service.glean.private.TimeUnit
import java.util.concurrent.TimeUnit as AndroidTimeUnit

/**
 * Convenience method to get a time in nanoseconds in a different, supported time unit.
 *
 * @param timeUnit the required time unit, one in [TimeUnit]
 * @param elapsedNanos a time in nanoseconds
 *
 * @return the time in the desired time unit
 */
internal fun getAdjustedTime(timeUnit: TimeUnit, elapsedNanos: Long): Long {
    return when (timeUnit) {
        TimeUnit.Nanosecond -> elapsedNanos
        TimeUnit.Microsecond -> AndroidTimeUnit.NANOSECONDS.toMicros(elapsedNanos)
        TimeUnit.Millisecond -> AndroidTimeUnit.NANOSECONDS.toMillis(elapsedNanos)
        TimeUnit.Second -> AndroidTimeUnit.NANOSECONDS.toSeconds(elapsedNanos)
        TimeUnit.Minute -> AndroidTimeUnit.NANOSECONDS.toMinutes(elapsedNanos)
        TimeUnit.Hour -> AndroidTimeUnit.NANOSECONDS.toHours(elapsedNanos)
        TimeUnit.Day -> AndroidTimeUnit.NANOSECONDS.toDays(elapsedNanos)
    }
}

/**
 * Convenience method to get a time in a different unit to nanoseconds.
 *
 * @param timeUnit the unit the value is in
 * @param value a time in the given unit
 *
 * @return the time, in nanoseconds
 */
internal fun timeToNanos(timeUnit: TimeUnit, value: Long): Long {
    return when (timeUnit) {
        TimeUnit.Nanosecond -> value
        TimeUnit.Microsecond -> AndroidTimeUnit.MICROSECONDS.toNanos(value)
        TimeUnit.Millisecond -> AndroidTimeUnit.MILLISECONDS.toNanos(value)
        TimeUnit.Second -> AndroidTimeUnit.SECONDS.toNanos(value)
        TimeUnit.Minute -> AndroidTimeUnit.MINUTES.toNanos(value)
        TimeUnit.Hour -> AndroidTimeUnit.HOURS.toNanos(value)
        TimeUnit.Day -> AndroidTimeUnit.DAYS.toNanos(value)
    }
}
