/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.CreditCard
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class CreditCardsFragmentStoreTest {

    private lateinit var creditCardsState: CreditCardsListState
    private lateinit var creditCardsStore: CreditCardsFragmentStore

    @Before
    fun setup() {
        creditCardsState = CreditCardsListState(creditCards = emptyList())
        creditCardsStore = CreditCardsFragmentStore(creditCardsState)
    }

    @Test
    fun testUpdateCreditCards() = runBlocking {
        assertTrue(creditCardsStore.state.isLoading)

        val creditCards: List<CreditCard> = listOf(mockk(), mockk())
        creditCardsStore.dispatch(CreditCardsAction.UpdateCreditCards(creditCards)).join()

        assertEquals(creditCards, creditCardsStore.state.creditCards)
        assertFalse(creditCardsStore.state.isLoading)
    }
}
