/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.content.Context;
import android.support.annotation.NonNull;

import org.mozilla.focus.architecture.NonNullObserver;

import java.util.List;

public class NotificationSessionObserver extends NonNullObserver<List<Session>> {
    private final Context context;

    public NotificationSessionObserver(Context context) {
        this.context = context;
    }

    @Override
    protected void onValueChanged(@NonNull List<Session> sessions) {
        if (sessions.isEmpty()) {
            SessionNotificationService.stop(context);
        } else {
            SessionNotificationService.start(context);
        }
    }
}
