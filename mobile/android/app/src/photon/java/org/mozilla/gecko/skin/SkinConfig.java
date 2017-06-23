/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.skin;

import android.support.annotation.IntDef;

public class SkinConfig {

    private static final int SKIN_AUSTRALIS = 0;
    private static final int SKIN_PHOTON = 1;

    @IntDef({ SKIN_AUSTRALIS, SKIN_PHOTON })
    private @interface Skin {}

    @SkinConfig.Skin
    private static final int SKIN = SKIN_PHOTON;

    public static boolean isAustralis() {
        return SKIN == SKIN_AUSTRALIS;
    }

    public static boolean isPhoton() {
        return SKIN == SKIN_PHOTON;
    }
}
