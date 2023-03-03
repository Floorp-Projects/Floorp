/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import android.content.Intent
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.nimbus.messaging.NimbusMessagingController
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDismissed

/**
 * Handles default interactions with the ui of GleanPlumb messages.
 */
class DefaultMessageController(
    private val appStore: AppStore,
    private val messagingController: NimbusMessagingController,
    private val homeActivity: HomeActivity,
) : MessageController {

    override fun onMessagePressed(message: Message) {
        val actionUri = messagingController.processMessageActionToUri(message)
        homeActivity.processIntent(Intent(Intent.ACTION_VIEW, actionUri))

        appStore.dispatch(MessageClicked(message))
    }

    override fun onMessageDismissed(message: Message) {
        appStore.dispatch(MessageDismissed(message))
    }
}
