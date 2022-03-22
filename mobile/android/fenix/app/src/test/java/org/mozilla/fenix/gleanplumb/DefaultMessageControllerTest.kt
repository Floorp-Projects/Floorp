/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.net.Uri
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDisplayed
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.nimbus.MessageData

@RunWith(FenixRobolectricTestRunner::class)
class DefaultMessageControllerTest {

    private val activity: HomeActivity = mockk(relaxed = true)
    private val storageNimbus: NimbusMessagingStorage = mockk(relaxed = true)
    private lateinit var controller: DefaultMessageController
    private val store: AppStore = mockk(relaxed = true)

    @Before
    fun setup() {
        controller = DefaultMessageController(
            messagingStorage = storageNimbus,
            appStore = store,
            homeActivity = activity
        )
    }

    @Test
    fun `WHEN calling onMessagePressed THEN update the store and handle the action`() {
        val customController = spyk(controller)
        every { customController.handleAction(any()) } returns mockk()

        val message = mockMessage()

        customController.onMessagePressed(message)

        verify { customController.handleAction(any()) }
        verify { store.dispatch(MessageClicked(message)) }
    }

    @Test
    fun `GIVEN an URL WHEN calling handleAction THEN process the intent with an open uri`() {
        val intent = controller.handleAction("http://mozilla.org")

        verify { activity.processIntent(any()) }

        val encodedUrl = Uri.encode("http://mozilla.org")
        assertEquals(
            "${BuildConfig.DEEP_LINK_SCHEME}://open?url=$encodedUrl",
            intent.data.toString()
        )
    }

    @Test
    fun `GIVEN an deeplink WHEN calling handleAction THEN process the intent with an deeplink uri`() {
        val intent = controller.handleAction("://settings_privacy")

        verify { activity.processIntent(any()) }

        assertEquals("${BuildConfig.DEEP_LINK_SCHEME}://settings_privacy", intent.data.toString())
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN report to the messageManager`() {
        val message = mockMessage()

        controller.onMessageDismissed(message)

        verify { store.dispatch(AppAction.MessagingAction.MessageDismissed(message)) }
    }

    @Test
    fun `WHEN calling onMessageDisplayed THEN report to the messageManager`() {
        val message = mockMessage()

        controller.onMessageDisplayed(message)

        verify { store.dispatch(MessageDisplayed(message)) }
    }

    private fun mockMessage() = Message(
        id = "id",
        data = MessageData(_context = testContext),
        style = mockk(),
        action = "action",
        triggers = emptyList(),
        metadata = Message.Metadata(
            id = "id",
            displayCount = 0,
            pressed = false,
            dismissed = false
        )
    )
}
