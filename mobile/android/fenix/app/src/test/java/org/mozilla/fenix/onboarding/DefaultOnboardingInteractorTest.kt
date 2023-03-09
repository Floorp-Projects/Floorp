/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.onboarding.controller.OnboardingController
import org.mozilla.fenix.onboarding.interactor.DefaultOnboardingInteractor

class DefaultOnboardingInteractorTest {

    private val controller: OnboardingController = mockk(relaxed = true)

    private lateinit var interactor: DefaultOnboardingInteractor

    @Before
    fun setup() {
        interactor = DefaultOnboardingInteractor(
            controller = controller,
        )
    }

    @Test
    fun `WHEN the onboarding is finished THEN forward to controller handler`() {
        interactor.onStartBrowsingClicked()
        verify { controller.handleStartBrowsingClicked() }
    }

    @Test
    fun `WHEN the privacy notice clicked THEN forward to controller handler`() {
        interactor.onReadPrivacyNoticeClicked()
        verify { controller.handleReadPrivacyNoticeClicked() }
    }
}
