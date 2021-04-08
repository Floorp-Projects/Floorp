/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isVisible
import io.mockk.mockk
import kotlinx.android.synthetic.main.component_credit_cards.view.*
import mozilla.components.concept.storage.CreditCard
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardsManagementInteractor
import org.mozilla.fenix.settings.creditcards.view.CreditCardsManagementView

@RunWith(FenixRobolectricTestRunner::class)
class CreditCardsManagementViewTest {

    private lateinit var view: ViewGroup
    private lateinit var interactor: CreditCardsManagementInteractor
    private lateinit var creditCardsView: CreditCardsManagementView

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(CreditCardsManagementView.LAYOUT_ID, null)
            .findViewById(R.id.credit_cards_wrapper)
        interactor = mockk(relaxed = true)

        creditCardsView = CreditCardsManagementView(view, interactor)
    }

    @Test
    fun testUpdate() {
        creditCardsView.update(CreditCardsListState(creditCards = emptyList()))

        assertTrue(view.progress_bar.isVisible)
        assertFalse(view.credit_cards_list.isVisible)

        val creditCards: List<CreditCard> = listOf(mockk(), mockk())
        creditCardsView.update(CreditCardsListState(
            creditCards = creditCards,
            isLoading = false
        ))

        assertFalse(view.progress_bar.isVisible)
        assertTrue(view.credit_cards_list.isVisible)
    }
}
