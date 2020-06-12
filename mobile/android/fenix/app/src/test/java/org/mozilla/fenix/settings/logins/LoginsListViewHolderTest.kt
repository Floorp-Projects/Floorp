/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.logins_item.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class LoginsListViewHolderTest {

    private val baseLogin = SavedLogin(
        guid = "abcd",
        origin = "mozilla.org",
        username = "admin",
        password = "password",
        timeLastUsed = 100L
    )

    private lateinit var view: View
    private lateinit var interactor: SavedLoginsInteractor

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(R.layout.logins_item, null)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `bind url and username`() {
        val holder = LoginsListViewHolder(view, interactor)
        holder.bind(baseLogin)

        assertEquals("mozilla.org", view.webAddressView.text)
        assertEquals("admin", view.usernameView.text)
    }

    @Test
    fun `call interactor on click`() {
        val holder = LoginsListViewHolder(view, interactor)
        holder.bind(baseLogin)

        view.performClick()
        verify { interactor.itemClicked(baseLogin) }
    }
}
