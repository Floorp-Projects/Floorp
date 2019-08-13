//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.CheckResult;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;

import org.mozilla.gecko.firstrun.PanelConfig;

import java.util.Map;


public interface MmaInterface {

    void init(Activity Activity, Map<String, ?> attributes);

    void setCustomIcon(@DrawableRes int iconResId);

    void start(Context context);

    void event(String mmaEvent);

    void event(String mmaEvent, double value);

    void stop();

    @CheckResult boolean handleGcmMessage(Context context, String from, Bundle bundle);

    void setDeviceId(@NonNull String deviceId);

    PanelConfig getPanelConfig(@NonNull Context context, PanelConfig.TYPE panelConfigType, final boolean useLocalValues);

    void listenOnceForVariableChanges(@NonNull final MmaDelegate.MmaVariablesChangedListener listener);
}
