/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import com.google.firebase.messaging.RemoteMessage
import mozilla.components.lib.push.firebase.AbstractFirebasePushService
import org.json.JSONObject
import org.mozilla.fenix.ext.components

class FirebasePush : AbstractFirebasePushService() {

    // Helper function to help determine if the incoming message is from Leanplum
    private fun RemoteMessage.isLeanplumChannel(): Boolean =
        data[LEANPLUM_KEY]
            ?.let { JSONObject(it) }
            ?.getString(LEANPLUM_NAME_KEY) == LEANPLUM_CHANNEL_NAME

    /**
     * Overrides onMessageReceived to handle incoming Leanplum messages.
     */
    override fun onMessageReceived(remoteMessage: RemoteMessage?) {
        if (remoteMessage?.isLeanplumChannel() == true) {
            val message = remoteMessage.data?.get(LEANPLUM_MESSAGE_KEY) ?: return
            sendLeanplumMessage(message)
            return
        }

        return super.onMessageReceived(remoteMessage)
    }

    private fun sendLeanplumMessage(message: String) {
        applicationContext.components.backgroundServices.notificationManager.showMessage(message)
    }

    companion object {
        private const val LEANPLUM_KEY = "lp_channel"
        private const val LEANPLUM_NAME_KEY = "name"
        private const val LEANPLUM_CHANNEL_NAME = "leanplum"
        private const val LEANPLUM_MESSAGE_KEY = "lp_message"
    }
}
