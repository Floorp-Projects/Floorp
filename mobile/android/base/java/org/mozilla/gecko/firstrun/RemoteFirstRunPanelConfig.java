/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.support.annotation.NonNull;

import org.mozilla.gecko.mma.MmaDelegate;

public class RemoteFirstRunPanelConfig implements FirstRunPanelConfigProviderStrategy {
    @Override
    public PanelConfig getPanelConfig(@NonNull Context context, PanelConfig.TYPE type, final boolean useLocalValues) {
        return MmaDelegate.getPanelConfig(context, type, useLocalValues);
    }
}
