/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.autofill

import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCard
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class AutofillFragmentStoreTest {

    private lateinit var state: AutofillFragmentState
    private lateinit var store: AutofillFragmentStore

    @Before
    fun setup() {
        state = AutofillFragmentState()
        store = AutofillFragmentStore(state)
    }

    @Test
    fun testUpdateCreditCards() = runTest {
        assertTrue(store.state.isLoading)

        val creditCards: List<CreditCard> = listOf(mockk(), mockk())
        store.dispatch(AutofillAction.UpdateCreditCards(creditCards)).join()

        assertEquals(creditCards, store.state.creditCards)
        assertFalse(store.state.isLoading)
    }

    @Test
    fun `GIVEN a list of addresses WHEN update addresses action is dispatched THEN addresses state is updated`() = runTest {
        assertTrue(store.state.isLoading)

        val addresses: List<Address> = listOf(mockk(), mockk())
        store.dispatch(AutofillAction.UpdateAddresses(addresses)).join()

        assertEquals(addresses, store.state.addresses)
        assertFalse(store.state.isLoading)
    }
}
