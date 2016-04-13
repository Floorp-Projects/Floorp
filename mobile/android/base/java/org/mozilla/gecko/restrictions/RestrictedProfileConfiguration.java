/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.UserManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
public class RestrictedProfileConfiguration implements RestrictionConfiguration {
    // Mapping from restrictable feature to default state (on/off)
    private static Map<Restrictable, Boolean> configuration = new LinkedHashMap<>();
    static {
        configuration.put(Restrictable.INSTALL_EXTENSION, false);
        configuration.put(Restrictable.PRIVATE_BROWSING, false);
        configuration.put(Restrictable.CLEAR_HISTORY, false);
        configuration.put(Restrictable.MASTER_PASSWORD, false);
        configuration.put(Restrictable.GUEST_BROWSING, false);
        configuration.put(Restrictable.ADVANCED_SETTINGS, false);
        configuration.put(Restrictable.CAMERA_MICROPHONE, false);
        configuration.put(Restrictable.DATA_CHOICES, false);
        configuration.put(Restrictable.BLOCK_LIST, false);
        configuration.put(Restrictable.TELEMETRY, false);
        configuration.put(Restrictable.HEALTH_REPORT, true);
        configuration.put(Restrictable.DEFAULT_THEME, true);
    }

    /**
     * These restrictions are hidden from the admin configuration UI.
     */
    private static List<Restrictable> hiddenRestrictions = new ArrayList<>();
    static {
        hiddenRestrictions.add(Restrictable.MASTER_PASSWORD);
        hiddenRestrictions.add(Restrictable.GUEST_BROWSING);
        hiddenRestrictions.add(Restrictable.DATA_CHOICES);
        hiddenRestrictions.add(Restrictable.DEFAULT_THEME);

        // Hold behind Nightly flag until we have an actual block list deployed.
        if (!AppConstants.NIGHTLY_BUILD) {
            hiddenRestrictions.add(Restrictable.BLOCK_LIST);
        }
    }

    /* package-private */ static boolean shouldHide(Restrictable restrictable) {
        return hiddenRestrictions.contains(restrictable);
    }

    /* package-private */ static Map<Restrictable, Boolean> getConfiguration() {
        return configuration;
    }

    private Context context;

    public RestrictedProfileConfiguration(Context context) {
        this.context = context.getApplicationContext();
    }

    @Override
    public synchronized boolean isAllowed(Restrictable restrictable) {
        // Special casing system/user restrictions
        if (restrictable == Restrictable.INSTALL_APPS || restrictable == Restrictable.MODIFY_ACCOUNTS) {
            return RestrictionCache.getUserRestriction(context, restrictable.name);
        }

        if (!RestrictionCache.hasApplicationRestriction(context, restrictable.name) && !configuration.containsKey(restrictable)) {
            // Always allow features that are not in the configuration
            return true;
        }

        return RestrictionCache.getApplicationRestriction(context, restrictable.name, configuration.get(restrictable));
    }

    @Override
    public boolean canLoadUrl(String url) {
        if (!isAllowed(Restrictable.INSTALL_EXTENSION) && AboutPages.isAboutAddons(url)) {
            return false;
        }

        if (!isAllowed(Restrictable.PRIVATE_BROWSING) && AboutPages.isAboutPrivateBrowsing(url)) {
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
        RestrictionCache.invalidate();
    }

    public static List<Restrictable> getVisibleRestrictions() {
        final List<Restrictable> visibleList = new ArrayList<>();

        for (Restrictable restrictable : configuration.keySet()) {
            if (hiddenRestrictions.contains(restrictable)) {
                continue;
            }
            visibleList.add(restrictable);
        }

        return visibleList;
    }
}
