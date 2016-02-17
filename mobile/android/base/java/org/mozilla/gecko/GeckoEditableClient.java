/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Handler;
import android.text.Editable;
import android.view.KeyEvent;

/**
 * Interface for the IC thread.
 */
interface GeckoEditableClient {
    void sendKeyEvent(KeyEvent event, int action, int metaState);
    Editable getEditable();
    void setBatchMode(boolean isBatchMode);
    void setSuppressKeyUp(boolean suppress);
    Handler setInputConnectionHandler(Handler handler);
    void postToInputConnection(Runnable runnable);
}
