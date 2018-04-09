/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

public final class GeckoRuntimeSettings implements Parcelable {
    private boolean mUseContentProcess;
    private String[] mArgs;
    private Bundle mExtras;

    /**
     * Initialize default settings.
     */
    public  GeckoRuntimeSettings() {
        this(null);
    }

    /* package */ GeckoRuntimeSettings(final @Nullable GeckoRuntimeSettings settings) {
        if (settings != null) {
            mUseContentProcess = settings.mUseContentProcess;
            mArgs = settings.mArgs.clone();
            mExtras = new Bundle(settings.mExtras);
        } else {
            mArgs = new String[0];
            mExtras = new Bundle();
        }
    }

    /**
     * Get the content process hint flag.
     *
     * @return The content process hint flag.
     */
    public boolean getUseContentProcessHint() {
        return mUseContentProcess;
    }

    /**
     * Get the custom Gecko process arguments.
     *
     * @return The Gecko process arguments.
     */
    public String[] getArguments() {
        return mArgs;
    }

    /**
     * Get the custom Gecko intent extras.
     *
     * @return The Gecko intent extras.
     */
    public Bundle getExtras() {
        return mExtras;
    }

    /**
     * Set the content process hint flag.
     *
     * @param use If true, this will reload the content process for future use.
     */
    public void setUseContentProcessHint(boolean use) {
        mUseContentProcess = use;
    }

    /**
     * Set the custom Gecko process arguments.
     *
     * @param args The Gecko process arguments.
     */
    public void setArguments(final @NonNull String[] args) {
        mArgs = args;
    }

    /**
     * Set the custom Gecko intent extras.
     *
     * @param extras The Gecko intent extras.
     */
    public void setExtras(final @NonNull Bundle extras) {
        mExtras = extras;
    }

    @Override // Parcelable
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    public void writeToParcel(Parcel out, int flags) {
        out.writeByte((byte) (mUseContentProcess ? 1 : 0));
        out.writeStringArray(mArgs);
        mExtras.writeToParcel(out, flags);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final Parcel source) {
        mUseContentProcess = source.readByte() == 1;
        mArgs = source.createStringArray();
        mExtras.readFromParcel(source);
    }

    public static final Parcelable.Creator<GeckoRuntimeSettings> CREATOR
        = new Parcelable.Creator<GeckoRuntimeSettings>() {
        @Override
        public GeckoRuntimeSettings createFromParcel(final Parcel in) {
            final GeckoRuntimeSettings settings = new GeckoRuntimeSettings();
            settings.readFromParcel(in);
            return settings;
        }

        @Override
        public GeckoRuntimeSettings[] newArray(final int size) {
            return new GeckoRuntimeSettings[size];
        }
    };
}
