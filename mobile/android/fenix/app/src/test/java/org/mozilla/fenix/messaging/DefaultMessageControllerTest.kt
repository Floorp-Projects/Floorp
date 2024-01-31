/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import android.content.Intent
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.nimbus.messaging.MessageData
import mozilla.components.service.nimbus.messaging.NimbusMessagingControllerInterface
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
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultMessageControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val messagingController: NimbusMessagingControllerInterface = mockk(relaxed = true)
    private lateinit var defaultMessageController: DefaultMessageController
    private val appStore: AppStore = mockk(relaxed = true)

    @Before
    fun setup() {
        defaultMessageController = DefaultMessageController(
            messagingController = messagingController,
            appStore = appStore,
            homeActivity = homeActivity,
        )
    }

    @Test
    fun `WHEN calling onMessagePressed THEN process the action intent and update the app store`() {
        val message = mockMessage()
        every { messagingController.getIntentForMessage(message) }.returns(Intent())

        defaultMessageController.onMessagePressed(message)

        verify { messagingController.getIntentForMessage(message) }
        verify { homeActivity.processIntent(any()) }
        verify { appStore.dispatch(MessageClicked(message)) }
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN update the app store`() {
        val message = mockMessage()

        defaultMessageController.onMessageDismissed(message)

        verify { appStore.dispatch(AppAction.MessagingAction.MessageDismissed(message)) }
    }

    private fun mockMessage(data: MessageData = MessageData()) = Message(
        id = "id",
        data = data,
        style = mockk(relaxed = true),
        action = "action",
        triggers = emptyList(),
        metadata = Message.Metadata(
            id = "id",
            displayCount = 0,
            pressed = false,
            dismissed = false,
        ),
    )
}
