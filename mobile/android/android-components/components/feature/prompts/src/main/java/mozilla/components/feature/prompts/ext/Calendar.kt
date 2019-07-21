/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.ext

import java.util.Calendar
import java.util.Date

internal fun Date.toCalendar() = Calendar.getInstance().also { it.time = this }

internal val Calendar.minute: Int
    get() = get(Calendar.MINUTE)
internal val Calendar.hour: Int
    get() = get(Calendar.HOUR_OF_DAY)
internal var Calendar.day: Int
    get() = get(Calendar.DAY_OF_MONTH)
    set(value) {
        set(Calendar.DAY_OF_MONTH, value)
    }
internal var Calendar.year: Int
    get() = get(Calendar.YEAR)
    set(value) {
        set(Calendar.YEAR, value)
    }

internal var Calendar.month: Int
    get() = get(Calendar.MONTH)
    set(value) {
        set(Calendar.MONTH, value)
    }

internal fun Calendar.minMonth(): Int = getMinimum(Calendar.MONTH)
internal fun Calendar.maxMonth(): Int = getActualMaximum(Calendar.MONTH)
internal fun Calendar.minDay(): Int = getMinimum(Calendar.DAY_OF_MONTH)
internal fun Calendar.maxDay(): Int = getActualMaximum(Calendar.DAY_OF_MONTH)
internal fun Calendar.minYear(): Int = getMinimum(Calendar.YEAR)
internal fun Calendar.maxYear(): Int = getActualMaximum(Calendar.YEAR)
internal fun now() = Calendar.getInstance()
