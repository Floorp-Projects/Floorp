//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;

import org.mozilla.gecko.firstrun.PanelConfig;

import java.util.Map;


public class MmaStubImp implements MmaInterface {
    @Override
    public void init(Activity activity, Map<String, ?> attributes) {

    }

    @Override
    public void setCustomIcon(@DrawableRes int iconResId) {

    }

    @Override
    public void start(Context context) {

    }

    @Override
    public void event(String leanplumEvent) {

    }

    @Override
    public void event(String leanplumEvent, double value) {

    }

    @Override
    public void stop() {

    }

    @Override
    public void setToken(String token) {

    }

    @Override
    public boolean handleGcmMessage(Context context, String from, Bundle bundle) {
        return false;
    }

    @Override
    public void setDeviceId(@NonNull String deviceId) {

    }

    @Override
    public PanelConfig getPanelConfig(@NonNull Context context, PanelConfig.TYPE panelConfigType, boolean useLocalValues) {
        return null;
    }

    @Override
    public void listenOnceForVariableChanges(@NonNull MmaDelegate.MmaVariablesChangedListener listener) {
        listener.onRemoteVariablesUnavailable();
    }
}
