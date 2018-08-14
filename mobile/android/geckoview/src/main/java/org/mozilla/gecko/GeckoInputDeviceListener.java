/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.hardware.input.InputManager;
import android.util.Log;
import android.view.InputDevice;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.InputDeviceUtils;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoInputDeviceListener
        implements InputManager.InputDeviceListener {
    private static final String LOGTAG = "GeckoInputDeviceListener";

    private static final GeckoInputDeviceListener listenerInstance = new GeckoInputDeviceListener();

    private boolean initialized;
    private InputManager mInputManager;

    public static GeckoInputDeviceListener getInstance() {
        return listenerInstance;
    }

    private GeckoInputDeviceListener() {
    }

    public synchronized void initialize(final Context context) {
        if (initialized) {
            Log.w(LOGTAG, "Already initialized!");
            return;
        }
        mInputManager = (InputManager)
            context.getSystemService(Context.INPUT_SERVICE);
        mInputManager.registerInputDeviceListener(listenerInstance, ThreadUtils.getUiHandler());
        initialized = true;
    }

    public synchronized void shutdown() {
        if (!initialized) {
            Log.w(LOGTAG, "Already shut down!");
            return;
        }

        if (mInputManager != null) {
            Log.e(LOGTAG, "mInputManager should be valid!");
            return;
        }

        mInputManager.unregisterInputDeviceListener(listenerInstance);
        initialized = false;
        mInputManager = null;
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
    private static native void onDeviceChanged();

    private void notifyDeviceChanged(int deviceId) {
        InputDevice device = InputDevice.getDevice(deviceId);
        if (device == null ||
            !InputDeviceUtils.isPointerTypeDevice(device)) {
            return;
        }
        onDeviceChanged();
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        notifyDeviceChanged(deviceId);
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        // Call onDeviceChanged directly without checking device source types
        // since we can no longer get a valid `InputDevice` in the case of
        // device removal.
        onDeviceChanged();
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        notifyDeviceChanged(deviceId);
    }
}
