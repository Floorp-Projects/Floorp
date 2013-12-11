/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.RobocopTarget;

import android.content.Context;
import android.os.Build;
import android.provider.Settings.Secure;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import java.util.Collection;
import java.util.Locale;

final public class InputMethods {
    public static final String METHOD_ANDROID_LATINIME = "com.android.inputmethod.latin/.LatinIME";
    public static final String METHOD_ATOK = "com.justsystems.atokmobile.service/.AtokInputMethodService";
    public static final String METHOD_GOOGLE_JAPANESE_INPUT = "com.google.android.inputmethod.japanese/.MozcService";
    public static final String METHOD_GOOGLE_LATINIME = "com.google.android.inputmethod.latin/com.android.inputmethod.latin.LatinIME";
    public static final String METHOD_HTC_TOUCH_INPUT = "com.htc.android.htcime/.HTCIMEService";
    public static final String METHOD_IWNN = "jp.co.omronsoft.iwnnime.ml/.standardcommon.IWnnLanguageSwitcher";
    public static final String METHOD_OPENWNN_PLUS = "com.owplus.ime.openwnnplus/.OpenWnnJAJP";
    public static final String METHOD_SAMSUNG = "com.sec.android.inputmethod/.SamsungKeypad";
    public static final String METHOD_SIMEJI = "com.adamrocker.android.input.simeji/.OpenWnnSimeji";
    public static final String METHOD_SWIFTKEY = "com.touchtype.swiftkey/com.touchtype.KeyboardService";
    public static final String METHOD_SWYPE = "com.swype.android.inputmethod/.SwypeInputMethod";
    public static final String METHOD_SWYPE_BETA = "com.nuance.swype.input/.IME";
    public static final String METHOD_TOUCHPAL_KEYBOARD = "com.cootek.smartinputv5/com.cootek.smartinput5.TouchPalIME";

    private InputMethods() {}

    public static String getCurrentInputMethod(Context context) {
        String inputMethod = Secure.getString(context.getContentResolver(), Secure.DEFAULT_INPUT_METHOD);
        return (inputMethod != null ? inputMethod : "");
    }

    public static InputMethodInfo getInputMethodInfo(Context context, String inputMethod) {
        InputMethodManager imm = getInputMethodManager(context);
        Collection<InputMethodInfo> infos = imm.getEnabledInputMethodList();
        for (InputMethodInfo info : infos) {
            if (info.getId().equals(inputMethod)) {
                return info;
            }
        }
        return null;
    }

    public static InputMethodManager getInputMethodManager(Context context) {
        return (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    public static boolean needsSoftResetWorkaround(String inputMethod) {
        // Stock latin IME on Android 4.2 and above
        return Build.VERSION.SDK_INT >= 17 && (METHOD_ANDROID_LATINIME.equals(inputMethod) ||
                                               METHOD_GOOGLE_LATINIME.equals(inputMethod));
    }

    public static boolean shouldCommitCharAsKey(String inputMethod) {
        return METHOD_HTC_TOUCH_INPUT.equals(inputMethod);
    }

    @RobocopTarget
    public static boolean shouldDisableUrlBarUpdate(Context context) {
        String inputMethod = getCurrentInputMethod(context);
        return METHOD_HTC_TOUCH_INPUT.equals(inputMethod);
    }

    public static boolean shouldDelayUrlBarUpdate(Context context) {
        String inputMethod = getCurrentInputMethod(context);
        return METHOD_SAMSUNG.equals(inputMethod) ||
               METHOD_SWIFTKEY.equals(inputMethod);
    }

    public static boolean isGestureKeyboard(Context context) {
        // SwiftKey is a gesture keyboard, but it doesn't seem to need any special-casing
        // to do AwesomeBar auto-spacing.
        String inputMethod = getCurrentInputMethod(context);
        return (Build.VERSION.SDK_INT >= 17 && (METHOD_ANDROID_LATINIME.equals(inputMethod) ||
                                                METHOD_GOOGLE_LATINIME.equals(inputMethod))) ||
               METHOD_SWYPE.equals(inputMethod) ||
               METHOD_SWYPE_BETA.equals(inputMethod) ||
               METHOD_TOUCHPAL_KEYBOARD.equals(inputMethod);
    }
}
