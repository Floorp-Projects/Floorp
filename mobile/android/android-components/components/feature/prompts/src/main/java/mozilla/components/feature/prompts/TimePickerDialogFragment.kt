/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.prompts

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Build
import android.os.Build.VERSION_CODES.M
import android.os.Bundle
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.View
import android.widget.DatePicker
import android.widget.TimePicker
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.appcompat.app.AlertDialog
import java.util.Calendar
import java.util.Date

private const val KEY_INITIAL_DATE = "KEY_INITIAL_DATE"
private const val KEY_MIN_DATE = "KEY_MIN_DATE"
private const val KEY_MAX_DATE = "KEY_MAX_DATE"
private const val KEY_SELECTED_DATE = "KEY_SELECTED_DATE"
private const val KEY_SELECTION_TYPE = "KEY_SELECTION_TYPE"

/**
 * [DialogFragment][androidx.fragment.app.DialogFragment] implementation to display date picker with a native dialog.
 */
@Suppress("TooManyFunctions")
internal class TimePickerDialogFragment : PromptDialogFragment(), DatePicker.OnDateChangedListener,
    TimePicker.OnTimeChangedListener {
    private val initialDate: Date by lazy { safeArguments.getSerializable(KEY_INITIAL_DATE) as Date }
    private val minimumDate: Date? by lazy { safeArguments.getSerializable((KEY_MIN_DATE)) as? Date }
    private val maximumDate: Date? by lazy { safeArguments.getSerializable(KEY_MAX_DATE) as? Date }
    private val selectionType: Int by lazy { safeArguments.getInt(KEY_SELECTION_TYPE) }

    @VisibleForTesting(otherwise = PRIVATE)
    internal var selectedDate: Date
        get() = safeArguments.getSerializable(KEY_SELECTED_DATE) as Date
        set(value) {
            safeArguments.putSerializable(KEY_SELECTED_DATE, value)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val inflater = LayoutInflater.from(requireContext())
        return AlertDialog.Builder(requireContext())
            .setCancelable(true)
            .setPositiveButton(R.string.mozac_feature_prompts_set_date) { _, _ -> onPositiveClickAction() }
            .setView(createDialogContentView(inflater))
            .setNegativeButton(R.string.mozac_feature_prompts_cancel) { _, _ -> feature?.onCancel(sessionId) }
            .setNeutralButton(R.string.mozac_feature_prompts_clear) { _, _ -> onClearClickAction() }
            .setupTitle()
            .create()
    }

    /**
     * Called when the user touches outside of the dialog.
     */
    override fun onCancel(dialog: DialogInterface?) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId)
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(inflater: LayoutInflater): View {
        return when (selectionType) {
            SELECTION_TYPE_DATE -> inflateDatePicker(inflater)
            SELECTION_TYPE_DATE_AND_TIME -> inflateDateTimePicker(inflater)
            SELECTION_TYPE_TIME -> inflateTimePicker(inflater)
            else -> {
                throw IllegalArgumentException("$selectionType is not a valid selectionType")
            }
        }
    }

    @SuppressLint("InflateParams")
    private fun inflateDatePicker(inflater: LayoutInflater): View {
        val view = inflater.inflate(R.layout.mozac_feature_prompts_date_picker, null)
        val datePicker = view.findViewById<DatePicker>(R.id.date_picker)
        bind(datePicker)
        return view
    }

    @SuppressLint("InflateParams")
    private fun inflateDateTimePicker(inflater: LayoutInflater): View {
        val view = inflater.inflate(R.layout.mozac_feature_prompts_date_time_picker, null)
        val datePicker = view.findViewById<DatePicker>(R.id.date_picker)
        val dateTimePicker = view.findViewById<TimePicker>(R.id.datetime_picker)
        val calendar = Calendar.getInstance()
        calendar.time = initialDate

        bind(datePicker)
        bind(dateTimePicker, calendar)

        return view
    }

    @SuppressLint("InflateParams")
    private fun inflateTimePicker(inflater: LayoutInflater): View {
        val view = inflater.inflate(R.layout.mozac_feature_prompts_time_picker, null)
        val dateTimePicker = view.findViewById<TimePicker>(R.id.time_picker)
        val calendar = Calendar.getInstance()
        calendar.time = initialDate

        bind(dateTimePicker, calendar)

        return view
    }

    @Suppress("DEPRECATION")
    private fun bind(picker: TimePicker, cal: Calendar) {
        if (Build.VERSION.SDK_INT >= M) {
            picker.hour = cal.get(Calendar.HOUR_OF_DAY)
            picker.minute = cal.get(Calendar.MINUTE)
        } else {
            picker.currentHour = cal.get(Calendar.HOUR_OF_DAY)
            picker.currentMinute = cal.get(Calendar.MINUTE)
        }
        picker.setIs24HourView(DateFormat.is24HourFormat(requireContext()))
        picker.setOnTimeChangedListener(this@TimePickerDialogFragment)
    }

    private fun bind(datePicker: DatePicker) {
        val calendar = Calendar.getInstance()
        calendar.time = initialDate

        minimumDate?.let {
            datePicker.minDate = it.time
        }
        maximumDate?.let {
            datePicker.maxDate = it.time
        }
        with(calendar) {
            datePicker.init(year, month, day, this@TimePickerDialogFragment)
        }
    }

    override fun onDateChanged(view: DatePicker?, year: Int, monthOfYear: Int, dayOfMonth: Int) {
        val calendar = Calendar.getInstance()
        calendar.set(year, monthOfYear, dayOfMonth)
        selectedDate = calendar.time
    }

    override fun onTimeChanged(picker: TimePicker, hourOfDay: Int, minute: Int) {
        val calendar = Calendar.getInstance()
        calendar.time = selectedDate

        calendar.set(Calendar.HOUR_OF_DAY, hourOfDay)
        calendar.set(Calendar.MINUTE, minute)
        selectedDate = calendar.time
    }

    private fun onClearClickAction() {
        feature?.onClear(sessionId)
    }

    private fun onPositiveClickAction() {
        feature?.onConfirm(sessionId, selectedDate)
    }

    companion object {
        /**
         * A builder method for creating a [TimePickerDialogFragment]
         * @param sessionId to create the dialog.
         * @param title of the dialog.
         * @param initialDate date that will be selected by default.
         * @param minDate the minimumDate date that will be allowed to be selected.
         * @param maxDate the maximumDate date that will be allowed to be selected.
         * @param selectionType indicate which type of time should be selected, valid values are
         * ([TimePickerDialogFragment.SELECTION_TYPE_DATE], [TimePickerDialogFragment.SELECTION_TYPE_DATE_AND_TIME],
         * and [TimePickerDialogFragment.SELECTION_TYPE_TIME])
         * @return a new instance of [TimePickerDialogFragment]
         */
        @Suppress("LongParameterList")
        fun newInstance(
            sessionId: String,
            title: String,
            initialDate: Date,
            minDate: Date?,
            maxDate: Date?,
            selectionType: Int = SELECTION_TYPE_DATE
        ): TimePickerDialogFragment {
            val fragment = TimePickerDialogFragment()
            val arguments = fragment.arguments ?: Bundle()
            fragment.arguments = arguments
            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putSerializable(KEY_INITIAL_DATE, initialDate)
                putSerializable(KEY_MIN_DATE, minDate)
                putSerializable(KEY_MAX_DATE, maxDate)
                putInt(KEY_SELECTION_TYPE, selectionType)
            }
            fragment.selectedDate = initialDate
            return fragment
        }

        const val SELECTION_TYPE_DATE = 1
        const val SELECTION_TYPE_DATE_AND_TIME = 2
        const val SELECTION_TYPE_TIME = 3
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun AlertDialog.Builder.setupTitle(): AlertDialog.Builder {
        val defaultTitle = when (selectionType) {
            SELECTION_TYPE_DATE -> R.string.mozac_feature_prompts_pick_a_date
            SELECTION_TYPE_DATE_AND_TIME -> R.string.mozac_feature_prompts_pick_a_date_and_time
            SELECTION_TYPE_TIME -> R.string.mozac_feature_prompts_pick_a_time
            else -> {
                throw IllegalArgumentException("$selectionType is not a valid selectionType")
            }
        }
        return if (title.isEmpty()) {
            setTitle(defaultTitle)
        } else {
            setTitle(title)
        }
    }
}

internal val Calendar.day: Int
    get() = get(Calendar.DAY_OF_MONTH)
internal val Calendar.year: Int
    get() = get(Calendar.YEAR)
internal val Calendar.month: Int
    get() = get(Calendar.MONTH)
