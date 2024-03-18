/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class DefaultCookieBannerDetailsInteractorTest {
    private lateinit var controller: CookieBannerDetailsController
    private lateinit var interactor: DefaultCookieBannerDetailsInteractor

    @Before
    fun setUp() {
        controller = mockk(relaxed = true)
        interactor = DefaultCookieBannerDetailsInteractor(controller)
    }

    @Test
    fun `WHEN onBackPressed is called THEN delegate the controller`() {
        interactor.onBackPressed()

        verify {
            controller.handleBackPressed()
        }
    }

    @Test
    fun `WHEN onTogglePressed is called THEN delegate the controller`() {
        interactor.onTogglePressed(true)

        verify {
            controller.handleTogglePressed(true)
        }
    }
}
