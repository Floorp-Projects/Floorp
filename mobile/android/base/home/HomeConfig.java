/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;

import java.util.EnumSet;
import java.util.List;

final class HomeConfig {
    /**
     * Used to determine what type of HomeFragment subclass to use when creating
     * a given panel. With the exception of DYNAMIC, all of these types correspond
     * to a default set of built-in panels. The DYNAMIC panel type is used by
     * third-party services to create panels with varying types of content.
     */
    public static enum PanelType implements Parcelable {
        TOP_SITES("top_sites", TopSitesPanel.class),
        BOOKMARKS("bookmarks", BookmarksPanel.class),
        HISTORY("history", HistoryPanel.class),
        READING_LIST("reading_list", ReadingListPanel.class),
        DYNAMIC("dynamic", DynamicPanel.class);

        private final String mId;
        private final Class<?> mPanelClass;

        PanelType(String id, Class<?> panelClass) {
            mId = id;
            mPanelClass = panelClass;
        }

        public static PanelType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to PanelType");
            }

            for (PanelType panelType : PanelType.values()) {
                if (TextUtils.equals(panelType.mId, id.toLowerCase())) {
                    return panelType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to PanelType");
        }

        @Override
        public String toString() {
            return mId;
        }

        public Class<?> getPanelClass() {
            return mPanelClass;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<PanelType> CREATOR = new Creator<PanelType>() {
            @Override
            public PanelType createFromParcel(final Parcel source) {
                return PanelType.values()[source.readInt()];
            }

            @Override
            public PanelType[] newArray(final int size) {
                return new PanelType[size];
            }
        };
    }

    public static class PanelConfig implements Parcelable {
        private final PanelType mType;
        private final String mTitle;
        private final String mId;
        private final EnumSet<Flags> mFlags;

        private static final String JSON_KEY_TYPE = "type";
        private static final String JSON_KEY_TITLE = "title";
        private static final String JSON_KEY_ID = "id";
        private static final String JSON_KEY_DEFAULT = "default";

        private static final int IS_DEFAULT = 1;

        public enum Flags {
            DEFAULT_PANEL
        }

        public PanelConfig(JSONObject json) throws JSONException, IllegalArgumentException {
            mType = PanelType.fromId(json.getString(JSON_KEY_TYPE));
            mTitle = json.getString(JSON_KEY_TITLE);
            mId = json.getString(JSON_KEY_ID);

            mFlags = EnumSet.noneOf(Flags.class);

            final boolean isDefault = (json.optInt(JSON_KEY_DEFAULT, -1) == IS_DEFAULT);
            if (isDefault) {
                mFlags.add(Flags.DEFAULT_PANEL);
            }
        }

        @SuppressWarnings("unchecked")
        public PanelConfig(Parcel in) {
            mType = (PanelType) in.readParcelable(getClass().getClassLoader());
            mTitle = in.readString();
            mId = in.readString();
            mFlags = (EnumSet<Flags>) in.readSerializable();
        }

        public PanelConfig(PanelType type, String title, String id) {
            this(type, title, id, EnumSet.noneOf(Flags.class));
        }

        public PanelConfig(PanelType type, String title, String id, EnumSet<Flags> flags) {
            if (type == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null type");
            }
            mType = type;

            if (title == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null title");
            }
            mTitle = title;

            if (id == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null id");
            }
            mId = id;

            if (flags == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null flags");
            }
            mFlags = flags;
        }

        public PanelType getType() {
            return mType;
        }

        public String getTitle() {
            return mTitle;
        }

        public String getId() {
            return mId;
        }

        public boolean isDefault() {
            return mFlags.contains(Flags.DEFAULT_PANEL);
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_TYPE, mType);
            json.put(JSON_KEY_TITLE, mTitle);
            json.put(JSON_KEY_ID, mId);

            if (mFlags.contains(Flags.DEFAULT_PANEL)) {
                json.put(JSON_KEY_DEFAULT, IS_DEFAULT);
            }

            return json;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeParcelable(mType, 0);
            dest.writeString(mTitle);
            dest.writeString(mId);
            dest.writeSerializable(mFlags);
        }

        public static final Creator<PanelConfig> CREATOR = new Creator<PanelConfig>() {
            @Override
            public PanelConfig createFromParcel(final Parcel in) {
                return new PanelConfig(in);
            }

            @Override
            public PanelConfig[] newArray(final int size) {
                return new PanelConfig[size];
            }
        };
    }

    public interface OnChangeListener {
        public void onChange();
    }

    public interface HomeConfigBackend {
        public List<PanelConfig> load();
        public void save(List<PanelConfig> entries);
        public void setOnChangeListener(OnChangeListener listener);
    }

    private final HomeConfigBackend mBackend;

    public HomeConfig(HomeConfigBackend backend) {
        mBackend = backend;
    }

    public List<PanelConfig> load() {
        return mBackend.load();
    }

    public void save(List<PanelConfig> entries) {
        mBackend.save(entries);
    }

    public void setOnChangeListener(OnChangeListener listener) {
        mBackend.setOnChangeListener(listener);
    }

    public static HomeConfig getDefault(Context context) {
        return new HomeConfig(new HomeConfigPrefsBackend(context));
    }
}
