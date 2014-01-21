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

import java.util.ArrayList;
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
        private final LayoutType mLayoutType;
        private final List<ViewConfig> mViews;
        private final EnumSet<Flags> mFlags;

        private static final String JSON_KEY_TYPE = "type";
        private static final String JSON_KEY_TITLE = "title";
        private static final String JSON_KEY_ID = "id";
        private static final String JSON_KEY_LAYOUT = "layout";
        private static final String JSON_KEY_VIEWS = "views";
        private static final String JSON_KEY_DEFAULT = "default";

        private static final int IS_DEFAULT = 1;

        public enum Flags {
            DEFAULT_PANEL
        }

        public PanelConfig(JSONObject json) throws JSONException, IllegalArgumentException {
            mType = PanelType.fromId(json.getString(JSON_KEY_TYPE));
            mTitle = json.getString(JSON_KEY_TITLE);
            mId = json.getString(JSON_KEY_ID);

            final String layoutTypeId = json.optString(JSON_KEY_LAYOUT, null);
            if (layoutTypeId != null) {
                mLayoutType = LayoutType.fromId(layoutTypeId);
            } else {
                mLayoutType = null;
            }

            final JSONArray jsonViews = json.optJSONArray(JSON_KEY_VIEWS);
            if (jsonViews != null) {
                mViews = new ArrayList<ViewConfig>();

                final int viewCount = jsonViews.length();
                for (int i = 0; i < viewCount; i++) {
                    final JSONObject jsonViewConfig = (JSONObject) jsonViews.get(i);
                    final ViewConfig viewConfig = new ViewConfig(jsonViewConfig);
                    mViews.add(viewConfig);
                }
            } else {
                mViews = null;
            }

            mFlags = EnumSet.noneOf(Flags.class);

            final boolean isDefault = (json.optInt(JSON_KEY_DEFAULT, -1) == IS_DEFAULT);
            if (isDefault) {
                mFlags.add(Flags.DEFAULT_PANEL);
            }

            validate();
        }

        @SuppressWarnings("unchecked")
        public PanelConfig(Parcel in) {
            mType = (PanelType) in.readParcelable(getClass().getClassLoader());
            mTitle = in.readString();
            mId = in.readString();
            mLayoutType = (LayoutType) in.readParcelable(getClass().getClassLoader());

            mViews = new ArrayList<ViewConfig>();
            in.readTypedList(mViews, ViewConfig.CREATOR);

            mFlags = (EnumSet<Flags>) in.readSerializable();

            validate();
        }

        public PanelConfig(PanelType type, String title, String id) {
            this(type, title, id, EnumSet.noneOf(Flags.class));
        }

        public PanelConfig(PanelType type, String title, String id, EnumSet<Flags> flags) {
            this(type, title, id, null, null, flags);
        }

        public PanelConfig(PanelType type, String title, String id, LayoutType layoutType,
                List<ViewConfig> views, EnumSet<Flags> flags) {
            mType = type;
            mTitle = title;
            mId = id;
            mFlags = flags;
            mLayoutType = layoutType;
            mViews = views;

            validate();
        }

        private void validate() {
            if (mType == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null type");
            }

            if (TextUtils.isEmpty(mTitle)) {
                throw new IllegalArgumentException("Can't create PanelConfig with empty title");
            }

            if (TextUtils.isEmpty(mId)) {
                throw new IllegalArgumentException("Can't create PanelConfig with empty id");
            }

            if (mLayoutType == null && mType == PanelType.DYNAMIC) {
                throw new IllegalArgumentException("Can't create a dynamic PanelConfig with null layout type");
            }

            if ((mViews == null || mViews.size() == 0) && mType == PanelType.DYNAMIC) {
                throw new IllegalArgumentException("Can't create a dynamic PanelConfig with no views");
            }

            if (mFlags == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null flags");
            }
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

        public LayoutType getLayoutType() {
            return mLayoutType;
        }

        public int getViewCount() {
            return (mViews != null ? mViews.size() : 0);
        }

        public ViewConfig getViewAt(int index) {
            return (mViews != null ? mViews.get(index) : null);
        }

        public boolean isDefault() {
            return mFlags.contains(Flags.DEFAULT_PANEL);
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_TYPE, mType.toString());
            json.put(JSON_KEY_TITLE, mTitle);
            json.put(JSON_KEY_ID, mId);

            if (mLayoutType != null) {
                json.put(JSON_KEY_LAYOUT, mLayoutType.toString());
            }

            if (mViews != null) {
                final JSONArray jsonViews = new JSONArray();

                final int viewCount = mViews.size();
                for (int i = 0; i < viewCount; i++) {
                    final ViewConfig viewConfig = mViews.get(i);
                    final JSONObject jsonViewConfig = viewConfig.toJSON();
                    jsonViews.put(jsonViewConfig);
                }

                json.put(JSON_KEY_VIEWS, jsonViews);
            }

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
            dest.writeParcelable(mLayoutType, 0);
            dest.writeTypedList(mViews);
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

    public static enum LayoutType implements Parcelable {
        FRAME("frame");

        private final String mId;

        LayoutType(String id) {
            mId = id;
        }

        public static LayoutType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to LayoutType");
            }

            for (LayoutType layoutType : LayoutType.values()) {
                if (TextUtils.equals(layoutType.mId, id.toLowerCase())) {
                    return layoutType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to LayoutType");
        }

        @Override
        public String toString() {
            return mId;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<LayoutType> CREATOR = new Creator<LayoutType>() {
            @Override
            public LayoutType createFromParcel(final Parcel source) {
                return LayoutType.values()[source.readInt()];
            }

            @Override
            public LayoutType[] newArray(final int size) {
                return new LayoutType[size];
            }
        };
    }

    public static enum ViewType implements Parcelable {
        LIST("list");

        private final String mId;

        ViewType(String id) {
            mId = id;
        }

        public static ViewType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to ViewType");
            }

            for (ViewType viewType : ViewType.values()) {
                if (TextUtils.equals(viewType.mId, id.toLowerCase())) {
                    return viewType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to ViewType");
        }

        @Override
        public String toString() {
            return mId;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<ViewType> CREATOR = new Creator<ViewType>() {
            @Override
            public ViewType createFromParcel(final Parcel source) {
                return ViewType.values()[source.readInt()];
            }

            @Override
            public ViewType[] newArray(final int size) {
                return new ViewType[size];
            }
        };
    }

    public static class ViewConfig implements Parcelable {
        private final ViewType mType;
        private final String mDatasetId;

        private static final String JSON_KEY_TYPE = "type";
        private static final String JSON_KEY_DATASET = "dataset";

        public ViewConfig(JSONObject json) throws JSONException, IllegalArgumentException {
            mType = ViewType.fromId(json.getString(JSON_KEY_TYPE));
            mDatasetId = json.getString(JSON_KEY_DATASET);

            validate();
        }

        @SuppressWarnings("unchecked")
        public ViewConfig(Parcel in) {
            mType = (ViewType) in.readParcelable(getClass().getClassLoader());
            mDatasetId = in.readString();

            validate();
        }

        public ViewConfig(ViewType type, String datasetId) {
            mType = type;
            mDatasetId = datasetId;

            validate();
        }

        private void validate() {
            if (mType == null) {
                throw new IllegalArgumentException("Can't create ViewConfig with null type");
            }

            if (TextUtils.isEmpty(mDatasetId)) {
                throw new IllegalArgumentException("Can't create ViewConfig with empty dataset ID");
            }
        }

        public ViewType getType() {
            return mType;
        }

        public String getDatasetId() {
            return mDatasetId;
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_TYPE, mType.toString());
            json.put(JSON_KEY_DATASET, mDatasetId);

            return json;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeParcelable(mType, 0);
            dest.writeString(mDatasetId);
        }

        public static final Creator<ViewConfig> CREATOR = new Creator<ViewConfig>() {
            @Override
            public ViewConfig createFromParcel(final Parcel in) {
                return new ViewConfig(in);
            }

            @Override
            public ViewConfig[] newArray(final int size) {
                return new ViewConfig[size];
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
