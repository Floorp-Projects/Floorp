/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.view

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_credit_cards.*
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.creditcards.CreditCardsListState
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardsManagementInteractor

/**
 * Shows a list of credit cards.
 */
class CreditCardsManagementView(
    override val containerView: ViewGroup,
    val interactor: CreditCardsManagementInteractor
) : LayoutContainer {

    private val creditCardsAdapter = CreditCardsAdapter(interactor)

    init {
        LayoutInflater.from(containerView.context).inflate(LAYOUT_ID, containerView, true)

        credit_cards_list.apply {
            adapter = creditCardsAdapter
            layoutManager = LinearLayoutManager(containerView.context)
        }

        add_credit_card_button.setOnClickListener { interactor.onAddCreditCardClick() }
    }

    /**
     * Updates the display of the credit cards based on the given [CreditCardsListState].
     */
    fun update(state: CreditCardsListState) {
        progress_bar.isVisible = state.isLoading
        credit_cards_list.isVisible = state.creditCards.isNotEmpty()

        creditCardsAdapter.submitList(state.creditCards)
    }

    companion object {
        const val LAYOUT_ID = R.layout.component_credit_cards
    }
}
