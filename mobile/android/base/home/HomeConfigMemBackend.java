/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig.HomeConfigBackend;
import org.mozilla.gecko.home.HomeConfig.OnChangeListener;
import org.mozilla.gecko.home.HomeConfig.PageEntry;
import org.mozilla.gecko.home.HomeConfig.PageType;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;

class HomeConfigMemBackend implements HomeConfigBackend {
    private final Context mContext;

    public HomeConfigMemBackend(Context context) {
        mContext = context;
    }

    public List<PageEntry> load() {
        final ArrayList<PageEntry> pageEntries = new ArrayList<PageEntry>();

        pageEntries.add(new PageEntry(PageType.TOP_SITES,
                                      mContext.getString(R.string.home_top_sites_title),
                                      EnumSet.of(PageEntry.Flags.DEFAULT_PAGE)));

        pageEntries.add(new PageEntry(PageType.BOOKMARKS,
                                      mContext.getString(R.string.bookmarks_title)));

        // We disable reader mode support on low memory devices. Hence the
        // reading list page should not show up on such devices.
        if (!HardwareUtils.isLowMemoryPlatform()) {
            pageEntries.add(new PageEntry(PageType.READING_LIST,
                                          mContext.getString(R.string.reading_list_title)));
        }

        final PageEntry historyEntry = new PageEntry(PageType.HISTORY,
                                                     mContext.getString(R.string.home_history_title));

        // On tablets, the history page is the last.
        // On phones, the history page is the first one.
        if (HardwareUtils.isTablet()) {
            pageEntries.add(historyEntry);
        } else {
            pageEntries.add(0, historyEntry);
        }

        return Collections.unmodifiableList(pageEntries);
    }

    public void save(List<PageEntry> entries) {
        // This is a static backend, do nothing.
    }

    public void setOnChangeListener(OnChangeListener listener) {
        // This is a static backend, do nothing.
    }
}