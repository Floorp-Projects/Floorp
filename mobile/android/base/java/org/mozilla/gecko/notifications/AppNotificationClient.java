/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.content.Context;

/**
 * Client for posting notifications in the application.
 */
public class AppNotificationClient extends NotificationClient {
    private final Context mContext;

    public AppNotificationClient(Context context) {
        mContext = context;
    }

    @Override
    protected void bind() {
        super.bind();
        connectHandler(new NotificationHandler(mContext));
    }
}
