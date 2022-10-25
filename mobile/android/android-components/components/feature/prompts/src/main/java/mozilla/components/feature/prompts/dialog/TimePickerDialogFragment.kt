/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.prompts.dialog

import android.annotation.SuppressLint
import android.app.AlertDialog
import android.app.DatePickerDialog
import android.app.Dialog
import android.app.TimePickerDialog
import android.content.Context
import android.content.DialogInterface
import android.content.DialogInterface.BUTTON_NEGATIVE
import android.content.DialogInterface.BUTTON_NEUTRAL
import android.content.DialogInterface.BUTTON_POSITIVE
import android.os.Build
import android.os.Build.VERSION_CODES.M
import android.os.Bundle
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.View
import android.widget.DatePicker
import android.widget.TimePicker
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.ext.day
import mozilla.components.feature.prompts.ext.hour
import mozilla.components.feature.prompts.ext.millisecond
import mozilla.components.feature.prompts.ext.minute
import mozilla.components.feature.prompts.ext.month
import mozilla.components.feature.prompts.ext.second
import mozilla.components.feature.prompts.ext.toCalendar
import mozilla.components.feature.prompts.ext.year
import mozilla.components.feature.prompts.widget.MonthAndYearPicker
import mozilla.components.feature.prompts.widget.TimePrecisionPicker
import mozilla.components.support.utils.TimePicker.shouldShowSecondsPicker
import java.util.Calendar
import java.util.Date

private const val KEY_INITIAL_DATE = "KEY_INITIAL_DATE"
private const val KEY_MIN_DATE = "KEY_MIN_DATE"
private const val KEY_MAX_DATE = "KEY_MAX_DATE"
private const val KEY_SELECTED_DATE = "KEY_SELECTED_DATE"
private const val KEY_SELECTION_TYPE = "KEY_SELECTION_TYPE"
private const val KEY_STEP_VALUE = "KEY_STEP_VALUE"

/**
 * [DialogFragment][androidx.fragment.app.DialogFragment] implementation to display date picker with a native dialog.
 */
internal class TimePickerDialogFragment :
    PromptDialogFragment(),
    DatePicker.OnDateChangedListener,
    TimePicker.OnTimeChangedListener,
    TimePickerDialog.OnTimeSetListener,
    DatePickerDialog.OnDateSetListener,
    DialogInterface.OnClickListener,
    MonthAndYearPicker.OnDateSetListener,
    TimePrecisionPicker.OnTimeSetListener {
    private val initialDate: Date by lazy { safeArguments.getSerializable(KEY_INITIAL_DATE) as Date }
    private val minimumDate: Date? by lazy { safeArguments.getSerializable((KEY_MIN_DATE)) as? Date }
    private val maximumDate: Date? by lazy { safeArguments.getSerializable(KEY_MAX_DATE) as? Date }
    private val selectionType: Int by lazy { safeArguments.getInt(KEY_SELECTION_TYPE) }
    private val stepSize: String? by lazy { safeArguments.getString(KEY_STEP_VALUE) }

    @VisibleForTesting(otherwise = PRIVATE)
    internal var selectedDate: Date
        get() = safeArguments.getSerializable(KEY_SELECTED_DATE) as Date
        set(value) {
            safeArguments.putSerializable(KEY_SELECTED_DATE, value)
        }

    @Suppress("ComplexMethod")
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val context = requireContext()
        val dialog = when (selectionType) {
            SELECTION_TYPE_TIME -> createTimePickerDialog(context)
            SELECTION_TYPE_DATE -> initialDate.toCalendar().let { cal ->
                DatePickerDialog(
                    context,
                    this@TimePickerDialogFragment,
                    cal.year,
                    cal.month,
                    cal.day,
                ).apply { setMinMaxDate(datePicker) }
            }
            SELECTION_TYPE_DATE_AND_TIME -> AlertDialog.Builder(context)
                .setView(inflateDateTimePicker(LayoutInflater.from(context)))
                .create()
                .also {
                    it.setButton(BUTTON_POSITIVE, context.getString(R.string.mozac_feature_prompts_set_date), this)
                    it.setButton(BUTTON_NEGATIVE, context.getString(R.string.mozac_feature_prompts_cancel), this)
                }
            SELECTION_TYPE_MONTH -> AlertDialog.Builder(context)
                .setTitle(R.string.mozac_feature_prompts_set_month)
                .setView(inflateDateMonthPicker())
                .create()
                .also {
                    it.setButton(BUTTON_POSITIVE, context.getString(R.string.mozac_feature_prompts_set_date), this)
                    it.setButton(BUTTON_NEGATIVE, context.getString(R.string.mozac_feature_prompts_cancel), this)
                }
            else -> throw IllegalArgumentException()
        }

        dialog.also {
            it.setCancelable(true)
            it.setButton(BUTTON_NEUTRAL, context.getString(R.string.mozac_feature_prompts_clear), this)
        }

        return dialog
    }

    /**
     * Called when the user touches outside of the dialog.
     */
    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        onClick(dialog, BUTTON_NEGATIVE)
    }

    // Create the appropriate time picker dialog for the given step value.
    private fun createTimePickerDialog(context: Context): AlertDialog {
        // Create the Android time picker dialog
        fun createTimePickerDialog(): AlertDialog {
            return initialDate.toCalendar().let { cal ->
                TimePickerDialog(
                    context,
                    this,
                    cal.hour,
                    cal.minute,
                    DateFormat.is24HourFormat(context),
                )
            }
        }

        // Create the custom time picker dialog
        fun createTimeStepPickerDialog(stepValue: Float): AlertDialog {
            return AlertDialog.Builder(context)
                .setTitle(R.string.mozac_feature_prompts_set_time)
                .setView(
                    TimePrecisionPicker(
                        context = requireContext(),
                        selectedTime = initialDate.toCalendar(),
                        maxTime = maximumDate?.toCalendar()
                            ?: MonthAndYearPicker.getDefaultMaxDate(),
                        minTime = minimumDate?.toCalendar()
                            ?: MonthAndYearPicker.getDefaultMinDate(),
                        stepValue = stepValue,
                        timeSetListener = this,
                    ),
                )
                .create()
                .also {
                    it.setButton(
                        BUTTON_POSITIVE,
                        context.getString(R.string.mozac_feature_prompts_set_date),
                        this,
                    )
                    it.setButton(
                        BUTTON_NEGATIVE,
                        context.getString(R.string.mozac_feature_prompts_cancel),
                        this,
                    )
                }
        }

        return if (!shouldShowSecondsPicker(stepSize?.toFloat())) {
            createTimePickerDialog()
        } else {
            stepSize?.let {
                createTimeStepPickerDialog(it.toFloat())
            } ?: createTimePickerDialog()
        }
    }

    @SuppressLint("InflateParams")
    private fun inflateDateTimePicker(inflater: LayoutInflater): View {
        val view = inflater.inflate(R.layout.mozac_feature_prompts_date_time_picker, null)
        val datePicker = view.findViewById<DatePicker>(R.id.date_picker)
        val dateTimePicker = view.findViewById<TimePicker>(R.id.datetime_picker)
        val cal = initialDate.toCalendar()

        // Bind date picker
        setMinMaxDate(datePicker)
        datePicker.init(cal.year, cal.month, cal.day, this)
        initTimePicker(dateTimePicker, cal)

        return view
    }

    private fun inflateDateMonthPicker(): View {
        return MonthAndYearPicker(
            context = requireContext(),
            selectedDate = initialDate.toCalendar(),
            maxDate = maximumDate?.toCalendar() ?: MonthAndYearPicker.getDefaultMaxDate(),
            minDate = minimumDate?.toCalendar() ?: MonthAndYearPicker.getDefaultMinDate(),
            dateSetListener = this,
        )
    }

    @Suppress("DEPRECATION")
    private fun initTimePicker(picker: TimePicker, cal: Calendar) {
        if (Build.VERSION.SDK_INT >= M) {
            picker.hour = cal.hour
            picker.minute = cal.minute
        } else {
            picker.currentHour = cal.hour
            picker.currentMinute = cal.minute
        }
        picker.setIs24HourView(DateFormat.is24HourFormat(requireContext()))
        picker.setOnTimeChangedListener(this)
    }

    private fun setMinMaxDate(datePicker: DatePicker) {
        minimumDate?.let {
            datePicker.minDate = it.time
        }
        maximumDate?.let {
            datePicker.maxDate = it.time
        }
    }

    override fun onTimeSet(
        picker: TimePrecisionPicker,
        hour: Int,
        minute: Int,
        second: Int,
        millisecond: Int,
    ) {
        val calendar = selectedDate.toCalendar()
        calendar.hour = hour
        calendar.minute = minute
        calendar.second = second
        calendar.millisecond = millisecond
        selectedDate = calendar.time
    }

    override fun onDateChanged(view: DatePicker?, year: Int, monthOfYear: Int, dayOfMonth: Int) {
        val calendar = Calendar.getInstance()
        calendar.set(year, monthOfYear, dayOfMonth)
        selectedDate = calendar.time
    }

    override fun onDateSet(view: DatePicker?, year: Int, month: Int, dayOfMonth: Int) {
        onDateChanged(view, year, month, dayOfMonth)
        onClick(null, BUTTON_POSITIVE)
    }

    override fun onDateSet(picker: MonthAndYearPicker, month: Int, year: Int) {
        onDateChanged(null, year, month, 0)
    }

    override fun onTimeChanged(picker: TimePicker?, hourOfDay: Int, minute: Int) {
        val calendar = selectedDate.toCalendar()
        calendar.set(Calendar.HOUR_OF_DAY, hourOfDay)
        calendar.set(Calendar.MINUTE, minute)
        selectedDate = calendar.time
    }

    override fun onTimeSet(view: TimePicker?, hourOfDay: Int, minute: Int) {
        onTimeChanged(view, hourOfDay, minute)
        onClick(null, BUTTON_POSITIVE)
    }

    override fun onClick(dialog: DialogInterface?, which: Int) {
        when (which) {
            BUTTON_POSITIVE -> feature?.onConfirm(sessionId, promptRequestUID, selectedDate)
            BUTTON_NEGATIVE -> feature?.onCancel(sessionId, promptRequestUID)
            BUTTON_NEUTRAL -> feature?.onClear(sessionId, promptRequestUID)
        }
    }

    companion object {
        /**
         * A builder method for creating a [TimePickerDialogFragment]
         * @param sessionId to create the dialog.
         * @param promptRequestUID identifier of the [PromptRequest] for which this dialog is shown.
         * @param shouldDismissOnLoad whether or not the dialog should automatically be dismissed
         * when a new page is loaded.
         * @param title of the dialog.
         * @param initialDate date that will be selected by default.
         * @param minDate the minimumDate date that will be allowed to be selected.
         * @param maxDate the maximumDate date that will be allowed to be selected.
         * @param selectionType indicate which type of time should be selected, valid values are
         * ([TimePickerDialogFragment.SELECTION_TYPE_DATE], [TimePickerDialogFragment.SELECTION_TYPE_DATE_AND_TIME],
         * and [TimePickerDialogFragment.SELECTION_TYPE_TIME])
         * @param stepValue value of time jumped whenever the time is incremented/decremented.
         *
         * @return a new instance of [TimePickerDialogFragment]
         */
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            initialDate: Date,
            minDate: Date?,
            maxDate: Date?,
            selectionType: Int = SELECTION_TYPE_DATE,
            stepValue: String? = null,
        ): TimePickerDialogFragment {
            val fragment = TimePickerDialogFragment()
            val arguments = fragment.arguments ?: Bundle()
            fragment.arguments = arguments
            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putSerializable(KEY_INITIAL_DATE, initialDate)
                putSerializable(KEY_MIN_DATE, minDate)
                putSerializable(KEY_MAX_DATE, maxDate)
                putString(KEY_STEP_VALUE, stepValue)
                putInt(KEY_SELECTION_TYPE, selectionType)
            }
            fragment.selectedDate = initialDate
            return fragment
        }

        const val SELECTION_TYPE_DATE = 1
        const val SELECTION_TYPE_DATE_AND_TIME = 2
        const val SELECTION_TYPE_TIME = 3
        const val SELECTION_TYPE_MONTH = 4
    }
}
