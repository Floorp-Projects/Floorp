/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_credit_card_editor.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

/**
 * Display a credit card editor for adding and editing a credit card.
 */
class CreditCardEditorFragment : Fragment(R.layout.fragment_credit_card_editor) {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        showToolbar(getString(R.string.credit_cards_add_card))

        setupButtonClickListeners(view)
        setupExpiryMonthDropDown(view)
        setupExpiryYearDropDown(view)
    }

    /**
     * Setup the all button click listeners in the credit card editor.
     */
    private fun setupButtonClickListeners(view: View) {
        view.cancel_button.setOnClickListener {
            findNavController().popBackStack()
        }
    }

    /**
     * Setup the expiry month dropdown by formatting and populating it with the months in a calendar
     * year.
     */
    private fun setupExpiryMonthDropDown(view: View) {
        val adapter =
            ArrayAdapter<String>(view.context, android.R.layout.simple_spinner_dropdown_item)
        val dateFormat = SimpleDateFormat("MMMM (MM)", Locale.getDefault())

        val calendar = Calendar.getInstance()
        calendar.set(Calendar.DAY_OF_MONTH, 1)

        for (month in 0..NUMBER_OF_MONTHS) {
            calendar.set(Calendar.MONTH, month)
            adapter.add(dateFormat.format(calendar.time))
        }

        view.expiry_month_drop_down.adapter = adapter
    }

    /**
     * Setup the expiry year dropdown with the latest 10 years.
     */
    private fun setupExpiryYearDropDown(view: View) {
        val adapter =
            ArrayAdapter<String>(view.context, android.R.layout.simple_spinner_dropdown_item)

        val calendar = Calendar.getInstance()
        val currentYear = calendar.get(Calendar.YEAR)

        for (year in currentYear until currentYear + NUMBER_OF_YEARS_TO_SHOW) {
            adapter.add(year.toString())
        }

        view.expiry_year_drop_down.adapter = adapter
    }

    companion object {
        // Number of months in a year (0-indexed).
        private const val NUMBER_OF_MONTHS = 11

        // Number of years to show in the expiry year dropdown.
        private const val NUMBER_OF_YEARS_TO_SHOW = 10
    }
}
