/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.prompts

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import android.support.v7.app.AlertDialog
import android.view.LayoutInflater
import android.view.View
import android.widget.DatePicker
import java.util.Calendar
import java.util.Date

private const val KEY_INITIAL_DATE = "KEY_INITIAL_DATE"
private const val KEY_MIN_DATE = "KEY_MIN_DATE"
private const val KEY_MAX_DATE = "KEY_MAX_DATE"
private const val KEY_SELECTED_DATE = "KEY_SELECTED_DATE"

/**
 * [android.support.v4.app.DialogFragment] implementation to display date picker with a native dialog.
 */
internal class DatePickerDialogFragment : PromptDialogFragment(), DatePicker.OnDateChangedListener {
    private val initialDate: Date by lazy { safeArguments.getSerializable(KEY_INITIAL_DATE) as Date }
    private val minimumDate: Date? by lazy { safeArguments.getSerializable((KEY_MIN_DATE)) as? Date }
    private val maximumDate: Date? by lazy { safeArguments.getSerializable(KEY_MAX_DATE) as? Date }
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
        val view = inflater.inflate(R.layout.mozac_feature_prompts_date_picker, null)
        val datePicker = view.findViewById<DatePicker>(R.id.date_picker)
        val calendar = Calendar.getInstance()
        calendar.time = initialDate
        minimumDate?.let {
            datePicker.minDate = it.time
        }
        maximumDate?.let {
            datePicker.maxDate = it.time
        }
        with(calendar) {
            datePicker.init(year, month, day, this@DatePickerDialogFragment)
        }
        return view
    }

    override fun onDateChanged(view: DatePicker?, year: Int, monthOfYear: Int, dayOfMonth: Int) {
        val calendar = Calendar.getInstance()
        calendar.set(year, monthOfYear, dayOfMonth)
        selectedDate = calendar.time
    }

    private fun onClearClickAction() {
        feature?.onClear(sessionId)
    }

    private fun onPositiveClickAction() {
        feature?.onSelect(sessionId, selectedDate)
    }

    companion object {
        /**
         * A builder method for creating a [DatePickerDialogFragment]
         * @param sessionId to create the dialog.
         * @param title of the dialog.
         * @param initialDate date that will be selected by default.
         * @param minDate the minimumDate date that will be allowed to be selected.
         * @param maxDate the maximumDate date that will be allowed to be selected.
         * @return a new instance of [DatePickerDialogFragment]
         */
        fun newInstance(
            sessionId: String,
            title: String,
            initialDate: Date,
            minDate: Date?,
            maxDate: Date?
        ): DatePickerDialogFragment {
            val fragment = DatePickerDialogFragment()
            val arguments = fragment.arguments ?: Bundle()
            fragment.arguments = arguments
            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_TITLE, title)
                putSerializable(KEY_INITIAL_DATE, initialDate)
                putSerializable(KEY_MIN_DATE, minDate)
                putSerializable(KEY_MAX_DATE, maxDate)
            }
            fragment.selectedDate = initialDate
            return fragment
        }
    }

    private fun AlertDialog.Builder.setupTitle(): AlertDialog.Builder {
        return if (title.isEmpty()) {
            setTitle(R.string.mozac_feature_prompts_pick_a_date)
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
