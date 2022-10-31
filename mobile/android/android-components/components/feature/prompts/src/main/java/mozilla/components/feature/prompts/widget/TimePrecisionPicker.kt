/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.widget

import android.annotation.SuppressLint
import android.content.Context
import android.view.View
import android.widget.NumberPicker
import android.widget.ScrollView
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.ext.hour
import mozilla.components.feature.prompts.ext.maxHour
import mozilla.components.feature.prompts.ext.maxMillisecond
import mozilla.components.feature.prompts.ext.maxMinute
import mozilla.components.feature.prompts.ext.maxSecond
import mozilla.components.feature.prompts.ext.millisecond
import mozilla.components.feature.prompts.ext.minHour
import mozilla.components.feature.prompts.ext.minMillisecond
import mozilla.components.feature.prompts.ext.minMinute
import mozilla.components.feature.prompts.ext.minSecond
import mozilla.components.feature.prompts.ext.minute
import mozilla.components.feature.prompts.ext.now
import mozilla.components.feature.prompts.ext.second
import mozilla.components.support.utils.TimePicker.shouldShowMillisecondsPicker
import java.util.Calendar

/**
 * UI widget that allows to select a time with precision of seconds or milliseconds.
 */
@SuppressLint("ViewConstructor") // This view is only instantiated in code
internal class TimePrecisionPicker @JvmOverloads constructor(
    context: Context,
    private val selectedTime: Calendar = now(),
    private val minTime: Calendar = getDefaultMinTime(),
    private val maxTime: Calendar = getDefaultMaxTime(),
    stepValue: Float,
    private var timeSetListener: OnTimeSetListener? = null,
) : ScrollView(context), NumberPicker.OnValueChangeListener {

    @VisibleForTesting
    internal val hourView: NumberPicker

    @VisibleForTesting
    internal val minuteView: NumberPicker

    @VisibleForTesting
    internal val secondView: NumberPicker

    @VisibleForTesting
    internal val millisecondView: NumberPicker

    init {
        inflate(context, R.layout.mozac_feature_prompts_time_picker, this)

        adjustMinMaxTimeIfInIllogicalRange()

        hourView = findViewById(R.id.hour_picker)
        minuteView = findViewById(R.id.minute_picker)
        secondView = findViewById(R.id.second_picker)
        millisecondView = findViewById(R.id.millisecond_picker)

        // Hide the millisecond picker if this is not the desired precision defined by the step
        if (!shouldShowMillisecondsPicker(stepValue)) {
            millisecondView.visibility = GONE
            findViewById<View>(R.id.millisecond_separator).visibility = GONE
        }

        initHourView()
        updateMinuteView()
        minuteView.setOnValueChangedListener(this)
        minuteView.setOnLongPressUpdateInterval(SPEED_MINUTE_SPINNER)
        secondView.init(
            selectedTime.second,
            selectedTime.minSecond(),
            selectedTime.maxSecond(),
        )
        millisecondView.init(
            selectedTime.millisecond,
            selectedTime.minMillisecond(),
            selectedTime.maxMillisecond(),
        )
    }

    override fun onValueChange(view: NumberPicker, oldVal: Int, newVal: Int) {
        var hour = selectedTime.hour
        var minute = selectedTime.minute
        var second = selectedTime.second
        var millisecond = selectedTime.millisecond

        when (view.id) {
            R.id.hour_picker -> {
                hour = newVal
                selectedTime.hour = hour
                updateMinuteView()
            }
            R.id.minute_picker -> {
                minute = newVal
                selectedTime.minute = minute
            }
            R.id.second_picker -> {
                second = newVal
                selectedTime.set(Calendar.SECOND, second)
            }
            R.id.millisecond_picker -> {
                millisecond = newVal
                selectedTime.set(Calendar.MILLISECOND, millisecond)
            }
        }

        // Update the selected time with the latest values.
        timeSetListener?.onTimeSet(this, hour, minute, second, millisecond)
    }

    private fun adjustMinMaxTimeIfInIllogicalRange() {
        // If the input time range is illogical, we should not restrict the input range.
        if (maxTime.before(minTime)) {
            minTime.timeInMillis = getDefaultMinTime().timeInMillis
            maxTime.timeInMillis = getDefaultMaxTime().timeInMillis
        }
    }

    // Initialize the hour view with the min and max values.
    private fun initHourView() {
        val min = minTime.hour
        val max = maxTime.hour

        if (selectedTime.hour < min || selectedTime.hour > max) {
            selectedTime.hour = min
            timeSetListener?.onTimeSet(
                this,
                selectedTime.hour,
                selectedTime.minute,
                selectedTime.second,
                selectedTime.millisecond,
            )
        }

        hourView.apply {
            minValue = min
            maxValue = max
            displayedValues = ((min..max).map { it.toString() }).toTypedArray()
            value = selectedTime.hour
            wrapSelectorWheel = true
            setOnValueChangedListener(this@TimePrecisionPicker)
            setOnLongPressUpdateInterval(SPEED_HOUR_SPINNER)
        }
    }

    // Update the minute view.
    private fun updateMinuteView() {
        val min: Int = if (selectedTime.hour == minTime.hour) {
            minTime.minute
        } else {
            selectedTime.minMinute()
        }
        val max: Int = if (selectedTime.hour == maxTime.hour) {
            maxTime.minute
        } else {
            selectedTime.maxMinute()
        }
        // If the hour is set to min/max value, then constraint the minute to a valid value.
        val minute = if (selectedTime.minute < min || selectedTime.minute > max) {
            timeSetListener?.onTimeSet(
                this,
                selectedTime.hour,
                min,
                selectedTime.second,
                selectedTime.millisecond,
            )
            min
        } else {
            selectedTime.minute
        }

        minuteView.apply {
            displayedValues = null
            minValue = min
            maxValue = max
            displayedValues = ((min..max).map { it.toString() }).toTypedArray()
            value = minute
            wrapSelectorWheel = true
        }
    }

    // Initialize the [NumberPicker].
    private fun NumberPicker.init(currentValue: Int, min: Int, max: Int) {
        minValue = min
        maxValue = max
        value = currentValue
        displayedValues = ((min..max).map { it.toString() }).toTypedArray()
        setOnValueChangedListener(this@TimePrecisionPicker)
        setOnLongPressUpdateInterval(SPEED_MINUTE_SPINNER)
    }

    // Interface used to set the selected time with seconds/milliseconds precision
    interface OnTimeSetListener {
        fun onTimeSet(
            picker: TimePrecisionPicker,
            hour: Int,
            minute: Int,
            second: Int,
            millisecond: Int,
        )
    }

    companion object {
        private const val SPEED_HOUR_SPINNER = 200L
        private const val SPEED_MINUTE_SPINNER = 100L

        internal fun getDefaultMinTime(): Calendar {
            return now().apply {
                hour = minHour()
                minute = minMinute()
                second = minSecond()
                millisecond = minMillisecond()
            }
        }

        internal fun getDefaultMaxTime(): Calendar {
            return now().apply {
                hour = maxHour()
                minute = maxMinute()
                second = maxSecond()
                millisecond = maxMillisecond()
            }
        }
    }
}
