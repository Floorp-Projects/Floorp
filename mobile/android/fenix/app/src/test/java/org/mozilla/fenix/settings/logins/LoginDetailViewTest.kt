/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.view.LayoutInflater
import android.view.ViewGroup
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.FragmentLoginDetailBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.logins.view.LoginDetailsBindingDelegate

@RunWith(FenixRobolectricTestRunner::class)
class LoginDetailViewTest {

    private val state = LoginsListState(
        loginList = emptyList(),
        filteredItems = emptyList(),
        currentItem = SavedLogin(
            guid = "abcd",
            origin = "mozilla.org",
            username = "admin",
            password = "password",
            timeLastUsed = 100L
        ),
        searchedForText = null,
        sortingStrategy = SortingStrategy.LastUsed,
        highlightedItem = SavedLoginsSortingStrategyMenu.Item.LastUsedSort,
        duplicateLogins = listOf()
    )

    private lateinit var view: ViewGroup
    private lateinit var binding: FragmentLoginDetailBinding
    private lateinit var detailsBindingDelegate: LoginDetailsBindingDelegate

    @Before
    fun setup() {
        binding = FragmentLoginDetailBinding.inflate(LayoutInflater.from(testContext))
        view = binding.loginDetailLayout
        detailsBindingDelegate = LoginDetailsBindingDelegate(binding)
    }

    @Test
    fun `bind currentItem`() {
        detailsBindingDelegate.update(state)

        assertEquals("mozilla.org", binding.webAddressText.text)
        assertEquals("admin", binding.usernameText.text)
        assertEquals("password", binding.passwordText.text)
    }

    @Test
    fun `bind null currentItem`() {
        detailsBindingDelegate.update(state.copy(currentItem = null))

        assertEquals("", binding.webAddressText.text)
        assertEquals("", binding.usernameText.text)
        assertEquals("", binding.passwordText.text)
    }
}
