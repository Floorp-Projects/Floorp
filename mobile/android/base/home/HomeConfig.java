/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomePager.Page;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;

import java.util.EnumSet;
import java.util.List;

final class HomeConfig {
    public static enum PageType implements Parcelable {
        TOP_SITES("top_sites", TopSitesPage.class),
        BOOKMARKS("bookmarks", BookmarksPage.class),
        HISTORY("history", HistoryPage.class),
        READING_LIST("reading_list", ReadingListPage.class),
        LIST("list", ListPage.class);

        private final String mId;
        private final Class<?> mPageClass;

        PageType(String id, Class<?> pageClass) {
            mId = id;
            mPageClass = pageClass;
        }

        public static PageType valueOf(Page page) {
            switch(page) {
                case TOP_SITES:
                    return PageType.TOP_SITES;

                case BOOKMARKS:
                    return PageType.BOOKMARKS;

                case HISTORY:
                    return PageType.HISTORY;

                case READING_LIST:
                    return PageType.READING_LIST;

                default:
                    throw new IllegalArgumentException("Could not convert unrecognized Page");
            }
        }

        public static PageType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to PageType");
            }

            for (PageType page : PageType.values()) {
                if (TextUtils.equals(page.mId, id.toLowerCase())) {
                    return page;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to PageType");
        }

        @Override
        public String toString() {
            return mId;
        }

        public Class<?> getPageClass() {
            return mPageClass;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<PageType> CREATOR = new Creator<PageType>() {
            @Override
            public PageType createFromParcel(final Parcel source) {
                return PageType.values()[source.readInt()];
            }

            @Override
            public PageType[] newArray(final int size) {
                return new PageType[size];
            }
        };
    }

    public static class PageEntry implements Parcelable {
        private final PageType mType;
        private final String mTitle;
        private final String mId;
        private final EnumSet<Flags> mFlags;

        public enum Flags {
            DEFAULT_PAGE
        }

        @SuppressWarnings("unchecked")
        public PageEntry(Parcel in) {
            mType = (PageType) in.readParcelable(getClass().getClassLoader());
            mTitle = in.readString();
            mId = in.readString();
            mFlags = (EnumSet<Flags>) in.readSerializable();
        }

        public PageEntry(PageType type, String title) {
            this(type, title, EnumSet.noneOf(Flags.class));
        }

        public PageEntry(PageType type, String title, EnumSet<Flags> flags) {
            this(type, title, type.toString(), flags);
        }

        public PageEntry(PageType type, String title, String id) {
            this(type, title, id, EnumSet.noneOf(Flags.class));
        }

        public PageEntry(PageType type, String title, String id, EnumSet<Flags> flags) {
            if (type == null) {
                throw new IllegalArgumentException("Can't create PageEntry with null type");
            }
            mType = type;

            if (title == null) {
                throw new IllegalArgumentException("Can't create PageEntry with null title");
            }
            mTitle = title;

            if (id == null) {
                throw new IllegalArgumentException("Can't create PageEntry with null id");
            }
            mId = id;

            if (flags == null) {
                throw new IllegalArgumentException("Can't create PageEntry with null flags");
            }
            mFlags = flags;
        }

        public PageType getType() {
            return mType;
        }

        public String getTitle() {
            return mTitle;
        }

        public String getId() {
            return mId;
        }

        public boolean isDefault() {
            return mFlags.contains(Flags.DEFAULT_PAGE);
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

        public static final Creator<PageEntry> CREATOR = new Creator<PageEntry>() {
            @Override
            public PageEntry createFromParcel(final Parcel in) {
                return new PageEntry(in);
            }

            @Override
            public PageEntry[] newArray(final int size) {
                return new PageEntry[size];
            }
        };
    }

    public interface OnChangeListener {
        public void onChange();
    }

    public interface HomeConfigBackend {
        public List<PageEntry> load();
        public void save(List<PageEntry> entries);
        public void setOnChangeListener(OnChangeListener listener);
    }

    private final HomeConfigBackend mBackend;

    public HomeConfig(HomeConfigBackend backend) {
        mBackend = backend;
    }

    public List<PageEntry> load() {
        return mBackend.load();
    }

    public void save(List<PageEntry> entries) {
        mBackend.save(entries);
    }

    public void setOnChangeListener(OnChangeListener listener) {
        mBackend.setOnChangeListener(listener);
    }

    public static HomeConfig getDefault(Context context) {
        return new HomeConfig(new HomeConfigMemBackend(context));
    }
}