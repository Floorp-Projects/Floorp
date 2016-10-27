/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.ScreenManagerHelper;

import android.content.Context;
import android.util.AttributeSet;
import android.util.DisplayMetrics;

public class PresentationView extends GeckoView {
    private static final String LOGTAG = "PresentationView";
    private static final String presentationViewURI = "chrome://browser/content/PresentationView.xul";

    public PresentationView(Context context, String deviceId, int screenId) {
        super(context);
        this.chromeURI = presentationViewURI + "#" + deviceId;
        this.screenId = screenId;
    }
}
