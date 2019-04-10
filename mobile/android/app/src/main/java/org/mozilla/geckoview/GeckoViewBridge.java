/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;

import org.mozilla.gecko.EventDispatcher;

/** This class exists so that Fennec can have access to getEventDispatcher without giving access to
 * it to any other GV consumer. */
public class GeckoViewBridge {
    @AnyThread
    public static @NonNull EventDispatcher getEventDispatcher(GeckoView geckoView) {
        return geckoView.getEventDispatcher();
    }
}
