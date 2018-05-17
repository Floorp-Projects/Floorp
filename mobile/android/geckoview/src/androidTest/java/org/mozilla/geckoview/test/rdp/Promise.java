/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.json.JSONObject;

/**
 * Object to represent a Promise object returned by JS.
 */
public final class Promise extends Grip {
    private JSONObject mGrip;
    private String mState;
    private Object mValue;
    private Object mReason;

    /* package */ Promise(final @NonNull RDPConnection connection, final @NonNull JSONObject grip) {
        super(connection, grip);
        setPromiseState(grip);
    }

    /* package */ void setPromiseState(final @NonNull JSONObject grip) {
        mGrip = grip;

        final JSONObject state = grip.optJSONObject("promiseState");
        mState = state.optString("state");
        if (isFulfilled()) {
            mValue = Grip.unpack(connection, state.opt("value"));
        } else if (isRejected()) {
            mReason = Grip.unpack(connection, state.opt("reason"));
        }
    }

    /**
     * Return whether this promise is pending.
     *
     * @return True if this promise is pending.
     */
    public boolean isPending() {
        return "pending".equals(mState);
    }

    /**
     * Return whether this promise is fulfilled.
     *
     * @return True if this promise is fulfilled.
     */
    public boolean isFulfilled() {
        return "fulfilled".equals(mState);
    }

    /**
     * Return the promise value, assuming it is fulfilled.
     *
     * @return Promise value.
     */
    public @Nullable Object getValue() {
        return mValue;
    }

    /**
     * Return whether this promise is rejected.
     *
     * @return True if this promise is rejected.
     */
    public boolean isRejected() {
        return "rejected".equals(mState);
    }

    /**
     * Return the promise reason, assuming it is rejected.
     *
     * @return Promise reason.
     */
    public @Nullable Object getReason() {
        return mReason;
    }

    /**
     * Get the value of a property in this promise object.
     *
     * @return Value or null if there is no such property.
     */
    public @Nullable Object getProperty(final @NonNull String name) {
        final JSONObject preview = mGrip.optJSONObject("preview");
        if (preview == null) {
            return null;
        }
        final JSONObject ownProperties = preview.optJSONObject("ownProperties");
        if (ownProperties == null) {
            return null;
        }
        final JSONObject prop = ownProperties.optJSONObject(name);
        if (prop == null) {
            return null;
        }
        return Grip.unpack(connection, prop.opt("value"));
    }

    @Override
    public String toString() {
        return "[Promise(" + mState + ")]" +
                (isFulfilled() ? "(" + mValue + ')' : "") +
                (isRejected() ? "(" + mReason + ')' : "");
    }
}
