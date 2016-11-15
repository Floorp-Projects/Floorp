/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONObject;

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

    // The order of the items here relate to image levels in
    // site_security_level.xml
    public enum SecurityMode {
        UNKNOWN("unknown"),
        IDENTIFIED("identified"),
        VERIFIED("verified"),
        CHROMEUI("chromeUI");

        private final String mId;

        private SecurityMode(String id) {
            mId = id;
        }

        public static SecurityMode fromString(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Can't convert null String to SiteIdentity");
            }

            for (SecurityMode mode : SecurityMode.values()) {
                if (TextUtils.equals(mode.mId, id)) {
                    return mode;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to SiteIdentity");
        }

        @Override
        public String toString() {
            return mId;
        }
    }

    // The order of the items here relate to image levels in
    // site_security_level.xml
    public enum MixedMode {
        UNKNOWN("unknown"),
        MIXED_CONTENT_BLOCKED("blocked"),
        MIXED_CONTENT_LOADED("loaded");

        private final String mId;

        private MixedMode(String id) {
            mId = id;
        }

        public static MixedMode fromString(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Can't convert null String to MixedMode");
            }

            for (MixedMode mode : MixedMode.values()) {
                if (TextUtils.equals(mode.mId, id.toLowerCase())) {
                    return mode;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to MixedMode");
        }

        @Override
        public String toString() {
            return mId;
        }
    }

    // The order of the items here relate to image levels in
    // site_security_level.xml
    public enum TrackingMode {
        UNKNOWN("unknown"),
        TRACKING_CONTENT_BLOCKED("tracking_content_blocked"),
        TRACKING_CONTENT_LOADED("tracking_content_loaded");

        private final String mId;

        private TrackingMode(String id) {
            mId = id;
        }

        public static TrackingMode fromString(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Can't convert null String to TrackingMode");
            }

            for (TrackingMode mode : TrackingMode.values()) {
                if (TextUtils.equals(mode.mId, id.toLowerCase())) {
                    return mode;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to TrackingMode");
        }

        @Override
        public String toString() {
            return mId;
        }
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
        mSecurityException = false;
    }

    void update(JSONObject identityData) {
        if (identityData == null) {
            reset();
            return;
        }

        try {
            JSONObject mode = identityData.getJSONObject("mode");

            try {
                mMixedModeDisplay = MixedMode.fromString(mode.getString("mixed_display"));
            } catch (Exception e) {
                mMixedModeDisplay = MixedMode.UNKNOWN;
            }

            try {
                mMixedModeActive = MixedMode.fromString(mode.getString("mixed_active"));
            } catch (Exception e) {
                mMixedModeActive = MixedMode.UNKNOWN;
            }

            try {
                mTrackingMode = TrackingMode.fromString(mode.getString("tracking"));
            } catch (Exception e) {
                mTrackingMode = TrackingMode.UNKNOWN;
            }

            try {
                mSecurityMode = SecurityMode.fromString(mode.getString("identity"));
            } catch (Exception e) {
                resetIdentity();
                return;
            }

            try {
                mOrigin = identityData.getString("origin");
                mHost = identityData.optString("host", null);
                mOwner = identityData.optString("owner", null);
                mSupplemental = identityData.optString("supplemental", null);
                mCountry = identityData.optString("country", null);
                mVerifier = identityData.optString("verifier", null);
                mSecure = identityData.optBoolean("secure", false);
                mSecurityException = identityData.optBoolean("securityException", false);

            } catch (Exception e) {
                resetIdentity();
            }
        } catch (Exception e) {
            reset();
        }
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
