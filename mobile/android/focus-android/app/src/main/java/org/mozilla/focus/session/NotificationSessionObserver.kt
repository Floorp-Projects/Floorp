/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import android.content.Context

import org.mozilla.focus.architecture.NonNullObserver

class NotificationSessionObserver(private val context: Context) : NonNullObserver<List<Session>>() {
    override fun onValueChanged(t: List<Session>) {
        if (t.isEmpty()) {
            SessionNotificationService.stop(context)
        } else {
            SessionNotificationService.start(context)
        }
    }
}
