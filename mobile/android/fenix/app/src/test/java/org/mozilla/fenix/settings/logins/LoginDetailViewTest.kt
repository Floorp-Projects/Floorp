/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.view.LayoutInflater
import android.view.ViewGroup
import io.mockk.mockk
import kotlinx.android.synthetic.main.fragment_login_detail.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

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
        sortingStrategy = SortingStrategy.LastUsed(mockk()),
        highlightedItem = SavedLoginsSortingStrategyMenu.Item.LastUsedSort
    )

    private lateinit var view: ViewGroup
    private lateinit var detailView: LoginDetailView

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(R.layout.fragment_login_detail, null)
            .findViewById(R.id.loginDetailLayout)
        detailView = LoginDetailView(view)
    }

    @Test
    fun `bind currentItem`() {
        detailView.update(state)

        assertEquals("mozilla.org", view.webAddressText.text)
        assertEquals("admin", view.usernameText.text)
        assertEquals("password", view.passwordText.text)
    }
}
