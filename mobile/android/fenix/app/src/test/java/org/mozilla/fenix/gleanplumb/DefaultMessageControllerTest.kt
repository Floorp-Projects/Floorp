/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.nimbus.MessageData

@RunWith(FenixRobolectricTestRunner::class)
class DefaultMessageControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val storageNimbus: NimbusMessagingStorage = mockk(relaxed = true)
    private val controllerNimbus: NimbusMessagingController = mockk(relaxed = true)
    private lateinit var controller: DefaultMessageController
    private val appStore: AppStore = mockk(relaxed = true)

    @Before
    fun setup() {
        controller = DefaultMessageController(
            messagingStorage = storageNimbus,
            messagingController = controllerNimbus,
            appStore = appStore,
            homeActivity = activity,
        )
    }

    @Test
    fun `WHEN calling onMessagePressed THEN update the app store and handle the action`() {
        val customController = spyk(controller)
        val message = mockMessage()

        customController.onMessagePressed(message)

        verify { controllerNimbus.processMessageAction(message) }
        verify { customController.handleAction(any()) }
        verify { appStore.dispatch(MessageClicked(message)) }
    }

    @Test
    fun `GIVEN an URL WHEN calling handleAction THEN process the intent with an open uri`() {
        val intent = controller.handleAction("http://mozilla.org")

        verify { activity.processIntent(any()) }

        assertEquals(
            "http://mozilla.org",
            intent.data.toString(),
        )
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN report to the messageManager`() {
        val message = mockMessage()

        controller.onMessageDismissed(message)

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
