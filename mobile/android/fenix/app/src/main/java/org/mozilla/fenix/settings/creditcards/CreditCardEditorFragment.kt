/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_credit_card_editor.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar

/**
 * Display a credit card editor for adding and editing a credit card.
 */
class CreditCardEditorFragment : Fragment(R.layout.fragment_credit_card_editor) {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        showToolbar(getString(R.string.credit_cards_add_card))

        setupButtonClickListeners(view)
    }

    /**
     * Setup the all button click listeners in the credit card editor.
     */
    private fun setupButtonClickListeners(view: View) {
        view.cancel_button.setOnClickListener {
            findNavController().popBackStack()
        }
    }
}
