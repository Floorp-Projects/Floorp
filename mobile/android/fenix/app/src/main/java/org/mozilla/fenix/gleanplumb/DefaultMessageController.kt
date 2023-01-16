/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDismissed

/**
 * Handles default interactions with the ui of GleanPlumb messages.
 */
class DefaultMessageController(
    private val appStore: AppStore,
    private val messagingStorage: NimbusMessagingStorage,
    private val homeActivity: HomeActivity,
) : MessageController {
    private val controller = NimbusMessagingController(messagingStorage)

    override fun onMessagePressed(message: Message) {
        val action = controller.processMessageAction(message)
        handleAction(action)
        appStore.dispatch(MessageClicked(message))
    }

    override fun onMessageDismissed(message: Message) {
        appStore.dispatch(MessageDismissed(message))
    }

    @VisibleForTesting
    internal fun handleAction(action: String): Intent {
        val intent = Intent(Intent.ACTION_VIEW, action.toUri())
        homeActivity.processIntent(intent)

        return intent
    }
}
