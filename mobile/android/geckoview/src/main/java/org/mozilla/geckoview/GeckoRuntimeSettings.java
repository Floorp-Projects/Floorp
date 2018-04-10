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
    /**
     * Settings builder used to construct the settings object.
     */
    public static final class Builder {
        private final GeckoRuntimeSettings mSettings;

        public Builder() {
            mSettings = new GeckoRuntimeSettings();
        }

        public Builder(final GeckoRuntimeSettings settings) {
            mSettings = new GeckoRuntimeSettings(settings);
        }

        /**
         * Finalize and return the settings.
         *
         * @return The constructed settings.
         */
        public @NonNull GeckoRuntimeSettings build() {
            return new GeckoRuntimeSettings(mSettings);
        }

        /**
         * Set the content process hint flag.
         *
         * @param use If true, this will reload the content process for future use.
         */
        public @NonNull Builder useContentProcessHint(final boolean use) {
            mSettings.mUseContentProcess = use;
            return this;
        }

        /**
         * Set the custom Gecko process arguments.
         *
         * @param args The Gecko process arguments.
         */
        public @NonNull Builder arguments(final @NonNull String[] args) {
            mSettings.mArgs = args;
            return this;
        }

        /**
         * Set the custom Gecko intent extras.
         *
         * @param extras The Gecko intent extras.
         */
        public @NonNull Builder extras(final @NonNull Bundle extras) {
            mSettings.mExtras = extras;
            return this;
        }
    }

    /* package */ GeckoRuntime runtime;
    /* package */ boolean mUseContentProcess;
    /* package */ String[] mArgs;
    /* package */ Bundle mExtras;
    /* package */ int prefCount;

    private class Pref<T> {
        public final String name;
        public final T defaultValue;
        private T value;

        public Pref(final String name, final T defaultValue) {
            GeckoRuntimeSettings.this.prefCount++;

            this.name = name;
            this.defaultValue = defaultValue;
            value = defaultValue;
        }

        public void set(T newValue) {
            value = newValue;
            flush();
        }

        public T get() {
            return value;
        }

        public void flush() {
            if (GeckoRuntimeSettings.this.runtime != null) {
                GeckoRuntimeSettings.this.runtime.setPref(name, value);
            }
        }
    }

    private final Pref<?>[] mPrefs = new Pref<?>[] {};

    /* package */ GeckoRuntimeSettings() {
        this(null);
    }

    /* package */ GeckoRuntimeSettings(final @Nullable GeckoRuntimeSettings settings) {
        if (BuildConfig.DEBUG && prefCount != mPrefs.length) {
            throw new AssertionError("Add new pref to prefs list");
        }

        if (settings == null) {
            mArgs = new String[0];
            mExtras = new Bundle();
        } else {
            mUseContentProcess = settings.getUseContentProcessHint();
            mArgs = settings.getArguments().clone();
            mExtras = new Bundle(settings.getExtras());
        }
    }

    /* package */ void flush() {
        for (final Pref<?> pref: mPrefs) {
            pref.flush();
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
