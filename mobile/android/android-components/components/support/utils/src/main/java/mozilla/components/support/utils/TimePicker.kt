/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

/**
 * Contains methods used to change the TimePickerDialog according to the step value.
 */
object TimePicker {

    private const val SHOW_SECONDS_PICKER_THRESHOLD = 60f
    private const val SHOW_MILLISECONDS_PICKER_THRESHOLD = 1f

    /**
     * Whether or not we should display the milliseconds picker
     * based on the value of the step attribute
     *
     * @param step Value of the step attribute
     */
    fun shouldShowMillisecondsPicker(step: Float?): Boolean =
        step != null && step < SHOW_MILLISECONDS_PICKER_THRESHOLD

    /**
     * Whether or not we should display the seconds picker
     * based on the value of the step attribute
     *
     * @param step Value of the step attribute
     */
    fun shouldShowSecondsPicker(step: Float?): Boolean =
        step != null && step < SHOW_SECONDS_PICKER_THRESHOLD
}
