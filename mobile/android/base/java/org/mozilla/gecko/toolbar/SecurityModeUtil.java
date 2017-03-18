/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.support.annotation.Nullable;

import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.MixedMode;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.SiteIdentity.TrackingMode;

import java.util.HashMap;
import java.util.Map;

/**
 * Util class which encapsulate logic of how CustomTabsActivity treats SiteIdentity.
 * TODO: Bug 1347037 - This class should be reusable for other components
 */
public class SecurityModeUtil {

    // defined basic mapping between SecurityMode and SecurityModeUtil.Mode
    private static final Map<SecurityMode, Mode> securityModeMap;

    static {
        securityModeMap = new HashMap<>();
        securityModeMap.put(SecurityMode.UNKNOWN, Mode.UNKNOWN);
        securityModeMap.put(SecurityMode.IDENTIFIED, Mode.LOCK_SECURE);
        securityModeMap.put(SecurityMode.VERIFIED, Mode.LOCK_SECURE);
        securityModeMap.put(SecurityMode.CHROMEUI, Mode.UNKNOWN);
    }

    /**
     * To resolve which mode to be used for given SiteIdentity. Its logic is similar to
     * ToolbarDisplayLayout.updateSiteIdentity
     *
     * @param identity An identity of a site to be resolved
     * @return Corresponding mode for resolved SiteIdentity, UNKNOWN as default.
     */
    public static Mode resolve(final @Nullable SiteIdentity identity) {
        if (identity == null) {
            return Mode.UNKNOWN;
        }

        final SecurityMode securityMode = identity.getSecurityMode();
        final MixedMode activeMixedMode = identity.getMixedModeActive();
        final MixedMode displayMixedMode = identity.getMixedModeDisplay();
        final TrackingMode trackingMode = identity.getTrackingMode();
        final boolean securityException = identity.isSecurityException();

        if (securityMode == SiteIdentity.SecurityMode.CHROMEUI) {
            return Mode.UNKNOWN;
        }

        if (securityException) {
            return Mode.MIXED_MODE;
        } else if (trackingMode == TrackingMode.TRACKING_CONTENT_LOADED) {
            return Mode.TRACKING_CONTENT_LOADED;
        } else if (trackingMode == TrackingMode.TRACKING_CONTENT_BLOCKED) {
            return Mode.TRACKING_CONTENT_BLOCKED;
        } else if (activeMixedMode == MixedMode.LOADED) {
            return Mode.MIXED_MODE;
        } else if (displayMixedMode == MixedMode.LOADED) {
            return Mode.WARNING;
        }

        return securityModeMap.containsKey(securityMode)
                ? securityModeMap.get(securityMode)
                : Mode.UNKNOWN;
    }

    // Security mode constants, which map to the icons / levels defined in:
    // http://dxr.mozilla.org/mozilla-central/source/mobile/android/base/java/org/mozilla/gecko/resources/drawable/customtabs_site_security_level.xml
    public enum Mode {
        UNKNOWN,
        LOCK_SECURE,
        WARNING,
        MIXED_MODE,
        TRACKING_CONTENT_BLOCKED,
        TRACKING_CONTENT_LOADED
    }
}
