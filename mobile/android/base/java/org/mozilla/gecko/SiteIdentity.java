/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoBundle;

import java.util.Locale;

import android.text.TextUtils;

public class SiteIdentity {
    private final String LOGTAG = "GeckoSiteIdentity";
    private SecurityMode mSecurityMode;
    private boolean mSecure;
    private MixedMode mMixedModeActive;
    private MixedMode mMixedModeDisplay;
    private TrackingMode mTrackingMode;
    private boolean mSecurityException;
    private String mHost;
    private String mOwner;
    private String mSupplemental;
    private String mCountry;
    private String mVerifier;
    private String mOrigin;

    public enum SecurityMode {
        UNKNOWN,
        IDENTIFIED,
        VERIFIED,
        CHROMEUI
    }

    public enum MixedMode {
        UNKNOWN,
        BLOCKED,
        LOADED
    }

    public enum TrackingMode {
        UNKNOWN,
        TRACKING_CONTENT_BLOCKED,
        TRACKING_CONTENT_LOADED
    }

    public SiteIdentity() {
        reset();
    }

    public void resetIdentity() {
        mSecurityMode = SecurityMode.UNKNOWN;
        mSecurityException = false;
        mOrigin = null;
        mHost = null;
        mOwner = null;
        mSupplemental = null;
        mCountry = null;
        mVerifier = null;
        mSecure = false;
    }

    public void reset() {
        resetIdentity();
        mMixedModeActive = MixedMode.UNKNOWN;
        mMixedModeDisplay = MixedMode.UNKNOWN;
        mTrackingMode = TrackingMode.UNKNOWN;
    }

    private <T extends Enum<T>> T getEnumValue(Class<T> enumType, String value, T def) {
        if (value == null) {
            return def;
        }
        try {
            return Enum.valueOf(enumType, value.toUpperCase(Locale.US));
        } catch (final IllegalArgumentException e) {
            return def;
        }
    }

    /* package */ void update(final GeckoBundle identityData) {
        if (identityData == null || !identityData.containsKey("mode")) {
            reset();
            return;
        }

        final GeckoBundle mode = identityData.getBundle("mode");

        mMixedModeDisplay = getEnumValue(
                MixedMode.class, mode.getString("mixed_display"), MixedMode.UNKNOWN);
        mMixedModeActive = getEnumValue(
                MixedMode.class, mode.getString("mixed_active"), MixedMode.UNKNOWN);
        mTrackingMode = getEnumValue(
                TrackingMode.class, mode.getString("tracking"), TrackingMode.UNKNOWN);

        if (!mode.containsKey("identity") || !identityData.containsKey("origin")) {
            resetIdentity();
            return;
        }

        mSecurityMode = getEnumValue(
                SecurityMode.class, mode.getString("identity"), SecurityMode.UNKNOWN);

        mOrigin = identityData.getString("origin");
        mHost = identityData.getString("host");
        mOwner = identityData.getString("owner");
        mSupplemental = identityData.getString("supplemental");
        mCountry = identityData.getString("country");
        mVerifier = identityData.getString("verifier");
        mSecure = identityData.getBoolean("secure");
        mSecurityException = identityData.getBoolean("securityException");
    }

    public SecurityMode getSecurityMode() {
        return mSecurityMode;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public String getHost() {
        return mHost;
    }

    public String getOwner() {
        return mOwner;
    }

    public boolean hasOwner() {
        return !TextUtils.isEmpty(mOwner);
    }

    public String getSupplemental() {
        return mSupplemental;
    }

    public String getCountry() {
        return mCountry;
    }

    public boolean hasCountry() {
        return !TextUtils.isEmpty(mCountry);
    }

    public String getVerifier() {
        return mVerifier;
    }

    public boolean isSecure() {
        return mSecure;
    }

    public MixedMode getMixedModeActive() {
        return mMixedModeActive;
    }

    public MixedMode getMixedModeDisplay() {
        return mMixedModeDisplay;
    }

    public boolean isSecurityException() {
        return mSecurityException;
    }

    public TrackingMode getTrackingMode() {
        return mTrackingMode;
    }
}
