/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.Completed
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class MicrosurveyMessageControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val message: Message = mockk(relaxed = true)
    private lateinit var microsurveyMessageController: MicrosurveyMessageController
    private val appStore: AppStore = mockk(relaxed = true)

    @Before
    fun setup() {
        microsurveyMessageController = MicrosurveyMessageController(
            appStore = appStore,
            homeActivity = homeActivity,
        )
    }

    @Test
    fun `WHEN calling onMessagePressed THEN update the app store with the MessageClicked action`() {
        microsurveyMessageController.onMessagePressed(message)
        verify { appStore.dispatch(MessageClicked(message)) }
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN update the app store with the MessageDismissed action`() {
        microsurveyMessageController.onMessageDismissed(message)

        verify { appStore.dispatch(AppAction.MessagingAction.MessageDismissed(message)) }
    }

    @Test
    fun `WHEN calling onPrivacyLinkClicked THEN open the privacy URL in a new tab`() {
        val privacyURL = "www.mozilla.com"
        microsurveyMessageController.onPrivacyLinkClicked(message, privacyURL)

        verify { homeActivity.openToBrowserAndLoad(any(), newTab = true, any(), any()) }
    }

    @Test
    fun `WHEN calling onSurveyCompleted THEN update the app store with the SurveyCompleted action`() {
        val answer = "satisfied"
        microsurveyMessageController.onSurveyCompleted(message, answer)

        verify { appStore.dispatch(Completed(message, answer)) }
    }
}
