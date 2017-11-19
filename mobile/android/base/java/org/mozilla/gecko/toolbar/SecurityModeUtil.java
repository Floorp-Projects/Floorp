/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.MixedMode;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.SiteIdentity.TrackingMode;

import java.util.HashMap;
import java.util.Map;

/**
 * Util class to help on resolving SiteIdentity to get corresponding visual result.
 */
public class SecurityModeUtil {

    /**
     * Abstract icon type for SiteIdentity resolving algorithm. Hence no need to worry about
     * Drawable level value.  Reference:
     * http://dxr.mozilla.org/mozilla-central/source/mobile/android/base/java/org/mozilla/gecko/resources/drawable/site_security_icon.xml
     */
    public enum IconType {
        UNKNOWN(0),
        DEFAULT(0),
        SEARCH(6),
        LOCK_SECURE(1),
        LOCK_WARNING(-1), // not used for now. reserve for MixedDisplayContent icon, if any.
        LOCK_INSECURE(3),
        WARNING(2),
        TRACKING_CONTENT_BLOCKED(4),
        TRACKING_CONTENT_LOADED(5);

        private final int imageLevel;

        private IconType(int imageLevel) {
            this.imageLevel = imageLevel;
        }

        /**
         * Get image level from IconType, and to use in ImageView.setImageLevel().
         *
         * @return the image level which defined in Drawable
         */
        public int getImageLevel() {
            return imageLevel;
        }
    }

    // defined basic mapping between SecurityMode and SecurityModeUtil.IconType
    private static final Map<SecurityMode, IconType> securityModeMap;

    static {
        securityModeMap = new HashMap<>();
        securityModeMap.put(SecurityMode.UNKNOWN, IconType.UNKNOWN);
        securityModeMap.put(SecurityMode.IDENTIFIED, IconType.LOCK_SECURE);
        securityModeMap.put(SecurityMode.VERIFIED, IconType.LOCK_SECURE);
        securityModeMap.put(SecurityMode.CHROMEUI, IconType.UNKNOWN);
    }

    /**
     * To resolve which icon-type to be used for given SiteIdentity.
     *
     * @param identity An identity of a site to be resolved
     * @return Corresponding type for resolved SiteIdentity, UNKNOWN as default.
     */
    public static IconType resolve(@Nullable final SiteIdentity identity) {
        return resolve(identity, null);
    }

    /**
     * To resolve which icon-type to be used for given SiteIdentity.
     *
     * @param identity An identity of a site to be resolved
     * @param url      current page url
     * @return Corresponding type for resolved SiteIdentity, UNKNOWN as default.
     */
    public static IconType resolve(@Nullable final SiteIdentity identity,
                                   @Nullable final String url) {

        if (!TextUtils.isEmpty(url) && AboutPages.isTitlelessAboutPage(url)) {
            // We always want to just show a search icon on about:home
            return IconType.SEARCH;
        }

        if (identity == null) {
            return IconType.UNKNOWN;
        }

        final SecurityMode securityMode = identity.getSecurityMode();
        final MixedMode activeMixedMode = identity.getMixedModeActive();
        final MixedMode displayMixedMode = identity.getMixedModeDisplay();
        final TrackingMode trackingMode = identity.getTrackingMode();
        final boolean securityException = identity.isSecurityException();

        if (securityException) {
            return IconType.WARNING;
        } else if (trackingMode == TrackingMode.TRACKING_CONTENT_LOADED) {
            return IconType.TRACKING_CONTENT_LOADED;
        } else if (trackingMode == TrackingMode.TRACKING_CONTENT_BLOCKED) {
            return IconType.TRACKING_CONTENT_BLOCKED;
        } else if (activeMixedMode == MixedMode.LOADED) {
            return IconType.LOCK_INSECURE;
        } else if (displayMixedMode == MixedMode.LOADED) {
            return IconType.WARNING;
        }

        // Chrome-UI checking is after tracking/mixed-content, even for about: pages, as they
        // can still load external sites.
        if (securityMode == SiteIdentity.SecurityMode.CHROMEUI) {
            return IconType.DEFAULT;
        }

        return securityModeMap.containsKey(securityMode)
                ? securityModeMap.get(securityMode)
                : IconType.UNKNOWN;
    }

    /**
     * For a given SiteIdentity, to check whether its tracking protection is enabled.
     *
     * @param identity to be checked
     * @return true if tracking protection is enabled.
     */
    public static boolean isTrackingProtectionEnabled(final @Nullable SiteIdentity identity) {
        final TrackingMode trackingMode = (identity == null)
                ? TrackingMode.UNKNOWN
                : identity.getTrackingMode();

        return (trackingMode == TrackingMode.TRACKING_CONTENT_BLOCKED);
    }
}
