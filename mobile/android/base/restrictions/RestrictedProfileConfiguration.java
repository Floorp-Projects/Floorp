/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.UserManager;

import java.util.Arrays;
import java.util.List;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
public class RestrictedProfileConfiguration implements RestrictionConfiguration {
    static List<Restrictable> DEFAULT_RESTRICTIONS = Arrays.asList(
            Restrictable.DISALLOW_INSTALL_EXTENSION,
            Restrictable.DISALLOW_PRIVATE_BROWSING,
            Restrictable.DISALLOW_LOCATION_SERVICE,
            Restrictable.DISALLOW_CLEAR_HISTORY,
            Restrictable.DISALLOW_MASTER_PASSWORD,
            Restrictable.DISALLOW_GUEST_BROWSING,
            Restrictable.DISALLOW_ADVANCED_SETTINGS,
            Restrictable.DISALLOW_CAMERA_MICROPHONE
    );

    private Context context;
    private Bundle cachedRestrictions;
    private boolean isCacheInvalid = true;

    public RestrictedProfileConfiguration(Context context) {
        this.context = context.getApplicationContext();
    }

    @Override
    public synchronized boolean isAllowed(Restrictable restrictable) {
        if (isCacheInvalid || !ThreadUtils.isOnUiThread()) {
            cachedRestrictions = readRestrictions();
            isCacheInvalid = false;
        }

        return !cachedRestrictions.getBoolean(restrictable.name, DEFAULT_RESTRICTIONS.contains(restrictable));
    }

    private Bundle readRestrictions() {
        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);

        StrictMode.ThreadPolicy policy = StrictMode.allowThreadDiskReads();

        try {
            Bundle restrictions = new Bundle();
            restrictions.putAll(mgr.getApplicationRestrictions(context.getPackageName()));
            restrictions.putAll(mgr.getUserRestrictions());
            return restrictions;
        } finally {
            StrictMode.setThreadPolicy(policy);
        }
    }

    @Override
    public boolean canLoadUrl(String url) {
        if (!isAllowed(Restrictable.DISALLOW_INSTALL_EXTENSION) && AboutPages.isAboutAddons(url)) {
            return false;
        }

        if (!isAllowed(Restrictable.DISALLOW_PRIVATE_BROWSING) && AboutPages.isAboutPrivateBrowsing(url)) {
            return false;
        }

        if (AboutPages.isAboutConfig(url)) {
            // Always block access to about:config to prevent circumventing restrictions (Bug 1189233)
            return false;
        }

        return true;
    }

    @Override
    public boolean isRestricted() {
        return true;
    }

    @Override
    public synchronized void update() {
        isCacheInvalid = true;
    }
}
