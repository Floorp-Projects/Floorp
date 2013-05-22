/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Build;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

public final class GamepadUtils {
    private static final int SONY_XPERIA_GAMEPAD_DEVICE_ID = 196611;

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

    public static KeyEvent translateSonyXperiaGamepadKeys(int keyCode, KeyEvent event) {
        // The cross and circle button mappings may be swapped in the different regions so
        // determine if they are swapped so the proper key codes can be mapped to the keys
        boolean areKeysSwapped = areSonyXperiaGamepadKeysSwapped();

        // If a Sony Xperia, remap the cross and circle buttons to buttons
        // A and B for the gamepad API
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
                keyCode = (areKeysSwapped ? KeyEvent.KEYCODE_BUTTON_A : KeyEvent.KEYCODE_BUTTON_B);
                break;

            case KeyEvent.KEYCODE_DPAD_CENTER:
                keyCode = (areKeysSwapped ? KeyEvent.KEYCODE_BUTTON_B : KeyEvent.KEYCODE_BUTTON_A);
                break;

            default:
                return event;
        }

        return new KeyEvent(event.getAction(), keyCode);
    }

    public static boolean isSonyXperiaGamepadKeyEvent(KeyEvent event) {
        return (event.getDeviceId() == SONY_XPERIA_GAMEPAD_DEVICE_ID &&
                "Sony Ericsson".equals(Build.MANUFACTURER) &&
                ("R800".equals(Build.MODEL) || "R800i".equals(Build.MODEL)));
    }

    private static boolean areSonyXperiaGamepadKeysSwapped() {
        // The cross and circle buttons on Sony Xperia phones are swapped
        // in different regions
        // http://developer.sonymobile.com/2011/02/13/xperia-play-game-keys/
        final char DEFAULT_O_BUTTON_LABEL = 0x25CB;

        boolean swapped = false;
        int[] deviceIds = InputDevice.getDeviceIds();

        for (int i= 0; deviceIds != null && i < deviceIds.length; i++) {
            KeyCharacterMap keyCharacterMap = KeyCharacterMap.load(deviceIds[i]);
            if (keyCharacterMap != null && DEFAULT_O_BUTTON_LABEL ==
                keyCharacterMap.getDisplayLabel(KeyEvent.KEYCODE_DPAD_CENTER)) {
                swapped = true;
                break;
            }
        }
        return swapped;
    }
}
