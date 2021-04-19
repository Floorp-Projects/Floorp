/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import androidx.fragment.app.FragmentActivity
import androidx.preference.Preference
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.concept.storage.CreditCard
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class CreditCardsSettingFragmentTest {

    private val testDispatcher = TestCoroutineDispatcher()
    private lateinit var creditCardsSettingFragment: CreditCardsSettingFragment

    @Before
    fun setUp() {
        creditCardsSettingFragment = CreditCardsSettingFragment()
        val activity = Robolectric.buildActivity(FragmentActivity::class.java).create().get()

        activity.supportFragmentManager.beginTransaction()
            .add(creditCardsSettingFragment, "CreditCardsSettingFragmentTest")
            .commitNow()
        testDispatcher.advanceUntilIdle()
    }

    @Test
    fun `GIVEN the list of credit cards is not empty, WHEN fragment is displayed THEN the manage credit cards pref is visible`() {
        val manageSavedCardsPreference = creditCardsSettingFragment.findPreference<Preference>(
            creditCardsSettingFragment.getPreferenceKey(R.string.pref_key_credit_cards_manage_saved_cards)
        )

        val creditCards: List<CreditCard> = listOf(mockk(), mockk())

        val creditCardsState = CreditCardsListState(creditCards = creditCards)
        val creditCardsStore = CreditCardsFragmentStore(creditCardsState)

        creditCardsSettingFragment.updateCardManagementPreferencesVisibility(creditCardsStore.state.creditCards)

        assertTrue(manageSavedCardsPreference!!.isVisible)
    }

    @Test
    fun `GIVEN the list of credit cards is empty, WHEN fragment is displayed THEN the add credit card pref is visible`() {
        val addCreditCardsPreference = creditCardsSettingFragment.findPreference<Preference>(
            creditCardsSettingFragment.getPreferenceKey(R.string.pref_key_credit_cards_add_credit_card)
        )

        val creditCardsState = CreditCardsListState(creditCards = emptyList())
        val creditCardsStore = CreditCardsFragmentStore(creditCardsState)

        creditCardsSettingFragment.updateCardManagementPreferencesVisibility(creditCardsStore.state.creditCards)

        assertTrue(addCreditCardsPreference!!.isVisible)
    }
}
