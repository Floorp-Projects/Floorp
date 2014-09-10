/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.prompts.Prompt;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ListView;

// Utility methods for entering/exiting guest mode.
public final class GuestSession {
    private static final String LOGTAG = "GeckoGuestSession";

    // Returns true if the user is using a secure keyguard, and its currently locked.
    static boolean isSecureKeyguardLocked(Context context) {
        final KeyguardManager manager = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);

        if (AppConstants.Versions.preJB) {
            return false;
        }

        return manager.isKeyguardLocked() && manager.isKeyguardSecure();
    }

    /* Returns true if you should be in guest mode. This can be because a secure keyguard
     * is locked, or because the user has explicitly started guest mode via a dialog. If the
     * user has explicitly started Fennec in guest mode, this will return true until they
     * explicitly exit it.
     */
    public static boolean shouldUse(final Context context, final String args) {
        // Did the command line args request guest mode?
        if (args != null && args.contains(BrowserApp.GUEST_BROWSING_ARG)) {
            return true;
        }

        // Otherwise, is the device locked?
        final boolean keyguard = isSecureKeyguardLocked(context);
        if (keyguard) {
            return true;
        }

        // Otherwise, is there a locked guest mode profile?
        final GeckoProfile profile = GeckoProfile.getGuestProfile(context);
        if (profile == null) {
            return false;
        }

        return profile.locked();
    }

    public static void configureWindow(Window window) {
        // In guest sessions we allow showing over the keyguard.
        window.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        window.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    }
}
