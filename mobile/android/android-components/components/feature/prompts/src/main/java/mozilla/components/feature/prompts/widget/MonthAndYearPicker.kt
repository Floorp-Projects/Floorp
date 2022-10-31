/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.widget

import android.annotation.SuppressLint
import android.content.Context
import android.widget.NumberPicker
import android.widget.ScrollView
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.ext.month
import mozilla.components.feature.prompts.ext.now
import mozilla.components.feature.prompts.ext.year
import java.util.Calendar

/**
 * UI widget that allows to select a month and a year.
 */
@SuppressLint("ViewConstructor") // This view is only instantiated in code
internal class MonthAndYearPicker @JvmOverloads constructor(
    context: Context,
    private val selectedDate: Calendar = now(),
    private val maxDate: Calendar = getDefaultMaxDate(),
    private val minDate: Calendar = getDefaultMinDate(),
    internal var dateSetListener: OnDateSetListener? = null,
) : ScrollView(context), NumberPicker.OnValueChangeListener {

    @VisibleForTesting
    internal val monthView: NumberPicker

    @VisibleForTesting
    internal val yearView: NumberPicker
    private val monthsLabels: Array<out String>

    init {
        inflate(context, R.layout.mozac_feature_promps_widget_month_picker, this)

        adjustMinMaxDateIfAreInIllogicalRange()
        adjustIfSelectedDateIsInIllogicalRange()

        monthsLabels = context.resources.getStringArray(R.array.mozac_feature_prompts_months)

        monthView = findViewById(R.id.month_chooser)
        yearView = findViewById(R.id.year_chooser)

        iniMonthView()
        iniYearView()
    }

    override fun onValueChange(view: NumberPicker, oldVal: Int, newVal: Int) {
        var month = 0
        var year = 0
        when (view.id) {
            R.id.month_chooser -> {
                month = newVal
                // Wrapping months to update greater fields
                if (oldVal == view.maxValue && newVal == view.minValue) {
                    yearView.value += 1
                    if (!yearView.value.isMinYear()) {
                        month = Calendar.JANUARY
                    }
                } else if (oldVal == view.minValue && newVal == view.maxValue) {
                    yearView.value -= 1
                    if (!yearView.value.isMaxYear()) {
                        month = Calendar.DECEMBER
                    }
                }
                year = yearView.value
            }
            R.id.year_chooser -> {
                month = monthView.value
                year = newVal
            }
        }

        selectedDate.month = month
        selectedDate.year = year
        updateMonthView(month)
        dateSetListener?.onDateSet(this, month + 1, year) // Month is zero based
    }

    private fun Int.isMinYear() = minDate.year == this
    private fun Int.isMaxYear() = maxDate.year == this

    private fun iniMonthView() {
        monthView.setOnValueChangedListener(this)
        monthView.setOnLongPressUpdateInterval(SPEED_MONTH_SPINNER)
        updateMonthView(selectedDate.month)
    }

    private fun iniYearView() {
        val year = selectedDate.year
        val max = maxDate.year
        val min = minDate.year

        yearView.init(year, min, max)
        yearView.wrapSelectorWheel = false
        yearView.setOnLongPressUpdateInterval(SPEED_YEAR_SPINNER)
    }

    private fun updateMonthView(month: Int) {
        var min = Calendar.JANUARY
        var max = Calendar.DECEMBER

        if (selectedDate.year.isMinYear()) {
            min = minDate.month
        }

        if (selectedDate.year.isMaxYear()) {
            max = maxDate.month
        }

        monthView.apply {
            displayedValues = null
            minValue = min
            maxValue = max
            displayedValues = monthsLabels.copyOfRange(monthView.minValue, monthView.maxValue + 1)
            value = month
            wrapSelectorWheel = true
        }
    }

    private fun adjustMinMaxDateIfAreInIllogicalRange() {
        // If the input date range is illogical/garbage, we should not restrict the input range (i.e. allow the
        // user to select any date). If we try to make any assumptions based on the illogical min/max date we could
        // potentially prevent the user from selecting dates that are in the developers intended range, so it's best
        // to allow anything.
        if (maxDate.before(minDate)) {
            minDate.timeInMillis = getDefaultMinDate().timeInMillis
            maxDate.timeInMillis = getDefaultMaxDate().timeInMillis
        }
    }

    private fun adjustIfSelectedDateIsInIllogicalRange() {
        if (selectedDate.before(minDate) || selectedDate.after(maxDate)) {
            selectedDate.timeInMillis = minDate.timeInMillis
        }
    }

    private fun NumberPicker.init(currentValue: Int, min: Int, max: Int) {
        minValue = min
        maxValue = max
        value = currentValue
        setOnValueChangedListener(this@MonthAndYearPicker)
    }

    interface OnDateSetListener {
        fun onDateSet(picker: MonthAndYearPicker, month: Int, year: Int)
    }

    companion object {

        private const val SPEED_MONTH_SPINNER = 200L
        private const val SPEED_YEAR_SPINNER = 100L

        @VisibleForTesting
        internal const val DEFAULT_MAX_YEAR = 9999

        @VisibleForTesting
        internal const val DEFAULT_MIN_YEAR = 1

        internal fun getDefaultMinDate(): Calendar {
            return now().apply {
                month = Calendar.JANUARY
                year = DEFAULT_MIN_YEAR
            }
        }

        internal fun getDefaultMaxDate(): Calendar {
            return now().apply {
                month = Calendar.DECEMBER
                year = DEFAULT_MAX_YEAR
            }
        }
    }
}
