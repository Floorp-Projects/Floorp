/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Handler;
import android.provider.Settings;
import android.util.Log;
import android.view.InputDevice;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.InputDeviceUtils;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoSystemStateListener
        implements InputManager.InputDeviceListener {
    private static final String LOGTAG = "GeckoSystemStateListener";

    private static final GeckoSystemStateListener listenerInstance = new GeckoSystemStateListener();

    private boolean initialized;
    private ContentObserver mContentObserver;
    private static Context sApplicationContext;
    private InputManager mInputManager;

    public static GeckoSystemStateListener getInstance() {
        return listenerInstance;
    }

    private GeckoSystemStateListener() {
    }

    public synchronized void initialize(final Context context) {
        if (initialized) {
            Log.w(LOGTAG, "Already initialized!");
            return;
        }
        mInputManager = (InputManager)
            context.getSystemService(Context.INPUT_SERVICE);
        mInputManager.registerInputDeviceListener(listenerInstance, ThreadUtils.getUiHandler());

        sApplicationContext = context;
        ContentResolver contentResolver = sApplicationContext.getContentResolver();
        Uri animationSetting = Settings.System.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE);
        mContentObserver = new ContentObserver(new Handler()) {
            @Override
            public void onChange(boolean selfChange) {
                onDeviceChanged();
            }
        };
        contentResolver.registerContentObserver(animationSetting, false, mContentObserver);

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

        ContentResolver contentResolver = sApplicationContext.getContentResolver();
        contentResolver.unregisterContentObserver(mContentObserver);

        initialized = false;
        mInputManager = null;
        mContentObserver = null;
    }

    @WrapForJNI(calledFrom = "gecko")
    // For prefers-reduced-motion media queries feature.
    private static boolean prefersReducedMotion() {
        ContentResolver contentResolver = sApplicationContext.getContentResolver();

        return Settings.Global.getFloat(contentResolver,
                                        Settings.Global.ANIMATOR_DURATION_SCALE,
                                        1) == 0.0f;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void notifyPrefersReducedMotionChangedForTest() {
        ContentResolver contentResolver = sApplicationContext.getContentResolver();
        Uri animationSetting = Settings.System.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE);
        contentResolver.notifyChange(animationSetting, null);
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
