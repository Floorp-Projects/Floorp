/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.os.Parcel;
import android.os.Parcelable;

public class TabHistoryPage implements Parcelable {
   private final String title;
   private final String url;
   private final boolean selected;

    public TabHistoryPage(String title, String url, boolean selected) {
        this.title = title;
        this.url = url;
        this.selected = selected;
    }

    public String getTitle() {
        return title;
    }

    public String getUrl() {
        return url;
    }

    public boolean isSelected() {
        return selected;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(title);
        dest.writeString(url);
        dest.writeInt(selected ? 1 : 0);
    }

    public static final Parcelable.Creator<TabHistoryPage> CREATOR = new Parcelable.Creator<TabHistoryPage>() {
        @Override
        public TabHistoryPage createFromParcel(final Parcel source) {
            final String title = source.readString();
            final String url = source.readString();
            final boolean selected = source.readByte() != 0;

            final TabHistoryPage page = new TabHistoryPage(title, url, selected);
            return page;
        }

        @Override
        public TabHistoryPage[] newArray(int size) {
            return new TabHistoryPage[size];
        }
    };
}
