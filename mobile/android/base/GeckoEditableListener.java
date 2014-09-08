/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

/**
 * Interface for the Editable to listen on the Gecko thread, as well as for the IC thread to listen
 * to the Editable.
 */
interface GeckoEditableListener {
    // IME notification type for notifyIME(), corresponding to NotificationToIME enum in Gecko
    int NOTIFY_IME_OPEN_VKB = -2;
    int NOTIFY_IME_REPLY_EVENT = -1;
    int NOTIFY_IME_OF_FOCUS = 1;
    int NOTIFY_IME_OF_BLUR = 2;
    int NOTIFY_IME_TO_COMMIT_COMPOSITION = 8;
    int NOTIFY_IME_TO_CANCEL_COMPOSITION = 9;
    // IME enabled state for notifyIMEContext()
    int IME_STATE_DISABLED = 0;
    int IME_STATE_ENABLED = 1;
    int IME_STATE_PASSWORD = 2;
    int IME_STATE_PLUGIN = 3;

    void notifyIME(int type);
    void notifyIMEContext(int state, String typeHint, String modeHint, String actionHint);
    void onSelectionChange(int start, int end);
    void onTextChange(CharSequence text, int start, int oldEnd, int newEnd);
}
