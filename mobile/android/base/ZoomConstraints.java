/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONException;
import org.json.JSONObject;

public final class ZoomConstraints {
    private final boolean mAllowZoom;
    private final boolean mAllowDoubleTapZoom;
    private final float mDefaultZoom;
    private final float mMinZoom;
    private final float mMaxZoom;

    public ZoomConstraints(boolean allowZoom) {
        mAllowZoom = allowZoom;
        mAllowDoubleTapZoom = allowZoom;
        mDefaultZoom = 0.0f;
        mMinZoom = 0.0f;
        mMaxZoom = 0.0f;
    }

    ZoomConstraints(JSONObject message) throws JSONException {
        mAllowZoom = message.getBoolean("allowZoom");
        mAllowDoubleTapZoom = message.getBoolean("allowDoubleTapZoom");
        mDefaultZoom = (float)message.getDouble("defaultZoom");
        mMinZoom = (float)message.getDouble("minZoom");
        mMaxZoom = (float)message.getDouble("maxZoom");
    }

    public final boolean getAllowZoom() {
        return mAllowZoom;
    }

    public final boolean getAllowDoubleTapZoom() {
        return mAllowDoubleTapZoom;
    }

    public final float getDefaultZoom() {
        return mDefaultZoom;
    }

    public final float getMinZoom() {
        return mMinZoom;
    }

    public final float getMaxZoom() {
        return mMaxZoom;
    }
}
