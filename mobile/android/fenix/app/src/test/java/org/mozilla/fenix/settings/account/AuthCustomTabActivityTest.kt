/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.content.Intent
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class AuthCustomTabActivityTest {

    @Test
    fun `navigateToBrowserOnColdStart does nothing for AuthCustomTabActivity`() {
        val activity = spyk(AuthCustomTabActivity())

        val settings: Settings = mockk()
        every { settings.shouldReturnToBrowser } returns true
        every { activity.components.settings.shouldReturnToBrowser } returns true
        every { activity.openToBrowser(any(), any()) } returns Unit

        activity.navigateToBrowserOnColdStart()

        verify(exactly = 0) { activity.openToBrowser(BrowserDirection.FromGlobal) }
    }

    @Test
    fun `navigateToHome does nothing for AuthCustomTabActivity`() {
        val activity = spyk(AuthCustomTabActivity())
        val navHostController: NavController = mockk()

        activity.navigateToHome(navHostController)
        verify { navHostController wasNot Called }
    }

    @Test
    fun `handleNewIntent does nothing for AuthCustomTabActivity`() {
        val activity = spyk(AuthCustomTabActivity())
        val intent: Intent = mockk(relaxed = true)

        activity.handleNewIntent(intent)
        verify { intent wasNot Called }
    }
}
