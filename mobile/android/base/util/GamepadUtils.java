/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Build;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

public final class GamepadUtils {
    private static View.OnKeyListener sClickDispatcher;
    private static View.OnKeyListener sListItemClickDispatcher;

    private GamepadUtils() {
    }

    private static boolean isGamepadKey(KeyEvent event) {
        if (Build.VERSION.SDK_INT >= 9) {
            return (event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD;
        }
        return false;
    }

    public static boolean isActionKey(KeyEvent event) {
        return (isGamepadKey(event) && (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A));
    }

    public static boolean isActionKeyDown(KeyEvent event) {
        return isActionKey(event) && event.getAction() == KeyEvent.ACTION_DOWN;
    }

    public static boolean isBackKey(KeyEvent event) {
        return (isGamepadKey(event) && (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B));
    }

    public static boolean isValueInDeadZone(MotionEvent event, int axis) {
        float value = event.getAxisValue(axis);
        // The 1e-2 here should really be range.getFlat() + range.getFuzz() (where range is
        // event.getDevice().getMotionRange(axis)), but the values those functions return
        // on the Ouya are zero so we're just hard-coding it for now.
        return (Math.abs(value) < 1e-2);
    }

    public static boolean isPanningControl(MotionEvent event) {
        if (Build.VERSION.SDK_INT < 12) {
            return false;
        }
        if ((event.getSource() & InputDevice.SOURCE_CLASS_MASK) != InputDevice.SOURCE_CLASS_JOYSTICK) {
            return false;
        }
        if (isValueInDeadZone(event, MotionEvent.AXIS_X)
                && isValueInDeadZone(event, MotionEvent.AXIS_Y)
                && isValueInDeadZone(event, MotionEvent.AXIS_Z)
                && isValueInDeadZone(event, MotionEvent.AXIS_RZ)) {
            return false;
        }
        return true;
    }

    public static View.OnKeyListener getClickDispatcher() {
        if (sClickDispatcher == null) {
            sClickDispatcher = new View.OnKeyListener() {
                @Override
                public boolean onKey(View v, int keyCode, KeyEvent event) {
                    if (isActionKeyDown(event)) {
                        return v.performClick();
                    }
                    return false;
                }
            };
        }
        return sClickDispatcher;
    }

    public static View.OnKeyListener getListItemClickDispatcher() {
        if (sListItemClickDispatcher == null) {
            sListItemClickDispatcher = new View.OnKeyListener() {
                @Override
                public boolean onKey(View v, int keyCode, KeyEvent event) {
                    if (isActionKeyDown(event) && (v instanceof ListView)) {
                        ListView view = (ListView)v;
                        AdapterView.OnItemClickListener listener = view.getOnItemClickListener();
                        if (listener != null) {
                            listener.onItemClick(view, view.getSelectedView(), view.getSelectedItemPosition(), view.getSelectedItemId());
                            return true;
                        }
                    }
                    return false;
                }
            };
        }
        return sListItemClickDispatcher;
    }
}
