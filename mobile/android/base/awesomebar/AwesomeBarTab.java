/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.view.View;
import android.widget.TextView;
import android.widget.ImageView;
import android.widget.TabHost.TabContentFactory;

abstract public class AwesomeBarTab {
    abstract public String getTag();
    abstract public int getTitleStringId();
    abstract public void destroy();
    abstract public TabContentFactory getFactory();

    // FIXME: This value should probably come from a prefs key
    public static final int MAX_RESULTS = 100;
    protected Context mContext = null;

    public AwesomeBarTab(Context context) {
        mContext = context;
    }

    protected class AwesomeEntryViewHolder {
        public TextView titleView;
        public TextView urlView;
        public ImageView faviconView;
        public ImageView bookmarkIconView;
    }
}
