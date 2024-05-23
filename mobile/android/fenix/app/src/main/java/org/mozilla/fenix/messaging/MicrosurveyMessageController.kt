/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import mozilla.components.service.nimbus.messaging.Message
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDismissed
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.Completed

/**
 * Handles interactions with the microsurveys.
 */
class MicrosurveyMessageController(
    private val appStore: AppStore,
    private val homeActivity: HomeActivity,
) : MessageController {

    override fun onMessagePressed(message: Message) {
        appStore.dispatch(MessageClicked(message))
    }

    override fun onMessageDismissed(message: Message) {
        appStore.dispatch(MessageDismissed(message))
    }

    /**
     * Handles the click event on the privacy link within a message.
     * @param message The message containing the privacy link.
     * @param privacyLink The URL of the privacy link.
     */
    // Suppress unused parameter to work around the CI.
    // message will be called by the UI when privacy noticed is clicked.
    @Suppress("UNUSED_PARAMETER")
    fun onPrivacyLinkClicked(message: Message, privacyLink: String) {
        homeActivity.openToBrowserAndLoad(
            searchTermOrURL = privacyLink,
            newTab = true,
            from = BrowserDirection.FromHome,
        )
    }

    /**
     * Dispatches an action when a survey is completed.
     * @param message The message containing the completed survey.
     * @param answer The answer provided in the survey.
     */
    fun onSurveyCompleted(message: Message, answer: String) {
        appStore.dispatch(Completed(message, answer))
    }
}
