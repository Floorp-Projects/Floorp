/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONObject;

import android.text.TextUtils;

public class SiteIdentity {
    private SecurityMode mSecurityMode;
    private MixedMode mMixedMode;
    private TrackingMode mTrackingMode;
    private String mHost;
    private String mOwner;
    private String mSupplemental;
    private String mVerifier;
    private String mEncrypted;

    // The order of the items here relate to image levels in
    // site_security_level.xml
    public enum SecurityMode {
        UNKNOWN("unknown"),
        IDENTIFIED("identified"),
        VERIFIED("verified");

        private final String mId;

        private SecurityMode(String id) {
            mId = id;
        }

        public static SecurityMode fromString(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Can't convert null String to SiteIdentity");
            }

            for (SecurityMode mode : SecurityMode.values()) {
                if (TextUtils.equals(mode.mId, id.toLowerCase())) {
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
        MIXED_CONTENT_BLOCKED("mixed_content_blocked"),
        MIXED_CONTENT_LOADED("mixed_content_loaded");

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
        mHost = null;
        mOwner = null;
        mSupplemental = null;
        mVerifier = null;
        mEncrypted = null;
    }

    public void reset() {
        resetIdentity();
        mMixedMode = MixedMode.UNKNOWN;
        mTrackingMode = TrackingMode.UNKNOWN;
    }

    void update(JSONObject identityData) {
        if (identityData == null) {
            reset();
            return;
        }

        try {
            JSONObject mode = identityData.getJSONObject("mode");

            try {
                mMixedMode = MixedMode.fromString(mode.getString("mixed"));
            } catch (Exception e) {
                mMixedMode = MixedMode.UNKNOWN;
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
                mHost = identityData.getString("host");
                mOwner = identityData.optString("owner", null);
                mSupplemental = identityData.optString("supplemental", null);
                mVerifier = identityData.getString("verifier");
                mEncrypted = identityData.getString("encrypted");
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

    public String getHost() {
        return mHost;
    }

    public String getOwner() {
        return mOwner;
    }

    public String getSupplemental() {
        return mSupplemental;
    }

    public String getVerifier() {
        return mVerifier;
    }

    public String getEncrypted() {
        return mEncrypted;
    }

    public MixedMode getMixedMode() {
        return mMixedMode;
    }

    public TrackingMode getTrackingMode() {
        return mTrackingMode;
    }
}
