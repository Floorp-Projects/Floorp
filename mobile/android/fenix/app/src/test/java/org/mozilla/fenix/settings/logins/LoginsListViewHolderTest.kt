/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.view.LayoutInflater
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.LoginsItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor
import org.mozilla.fenix.settings.logins.view.LoginsListViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class LoginsListViewHolderTest {

    private val baseLogin = SavedLogin(
        guid = "abcd",
        origin = "https://www.mozilla.org",
        username = "admin",
        password = "password",
        timeLastUsed = 100L,
    )

    private lateinit var interactor: SavedLoginsInteractor
    private lateinit var binding: LoginsItemBinding
    private lateinit var holder: LoginsListViewHolder

    @Before
    fun setup() {
        binding = LoginsItemBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
        holder = LoginsListViewHolder(
            binding.root,
            interactor,
        )
        every { testContext.components.core.icons } returns BrowserIcons(testContext, mockk(relaxed = true))
    }

    @Test
    fun `bind url and username`() {
        holder.bind(baseLogin)

        assertEquals("mozilla.org", binding.webAddressView.text)
        assertEquals("admin", binding.usernameView.text)
    }

    @Test
    fun `GIVEN url has a mobile prefix WHEN url is binded THEN mobile prefix is stripped`() {
        holder.bind(baseLogin.copy(origin = "https://m.mozilla.org"))

        assertEquals("mozilla.org", binding.webAddressView.text)

        holder.bind(baseLogin.copy(origin = "https://mobile.mozilla.org"))

        assertEquals("mozilla.org", binding.webAddressView.text)
    }

    @Test
    fun `call interactor on click`() {
        holder.bind(baseLogin)

        binding.root.performClick()
        verify { interactor.onItemClicked(baseLogin) }
    }
}
