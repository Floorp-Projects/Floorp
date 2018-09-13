/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.view.InputDevice;

public class InputDeviceUtils {
    public static boolean isPointerTypeDevice(InputDevice inputDevice) {
        int sources = inputDevice.getSources();
        return (sources & (InputDevice.SOURCE_CLASS_JOYSTICK |
                           InputDevice.SOURCE_CLASS_POINTER |
                           InputDevice.SOURCE_CLASS_POSITION |
                           InputDevice.SOURCE_CLASS_TRACKBALL)) != 0;
    }
}
