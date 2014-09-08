/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Handler;
import android.text.Editable;

/**
 * Interface for the IC thread.
 */
interface GeckoEditableClient {
    void sendEvent(GeckoEvent event);
    Editable getEditable();
    void setUpdateGecko(boolean update);
    void setSuppressKeyUp(boolean suppress);
    Handler getInputConnectionHandler();
    boolean setInputConnectionHandler(Handler handler);
}
