/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.Favicons;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SessionParser;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;
import org.mozilla.gecko.widget.FaviconView;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.ArrayList;

public class LastTabsSection extends AboutHomeSection {
    private Context mContext;

    private class TabInfo {
        final String url;
        final String title;
        final Bitmap favicon;

        TabInfo(String url, String title, Bitmap favicon) {
            this.url = url;
            this.title = title;
            this.favicon = favicon;
        }
    }

    public LastTabsSection(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void readLastTabs() {
        new UiAsyncTask<Void, Void, ArrayList<TabInfo>>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected ArrayList<TabInfo> doInBackground(Void... params) {
                String jsonString = GeckoProfile.get(mContext).readSessionFile(true);
                if (jsonString == null) {
                    // no previous session data
                    return null;
                }

                final ArrayList<TabInfo> lastTabs = new ArrayList<TabInfo>();
                new SessionParser() {
                    @Override
                    public void onTabRead(final SessionTab tab) {
                        final String url = tab.getSelectedUrl();
                        // don't show last tabs for about:home
                        if (url.equals("about:home")) {
                            return;
                        }

                        ContentResolver resolver = mContext.getContentResolver();
                        final Bitmap favicon = BrowserDB.getFaviconForUrl(resolver, url);
                        final String title = tab.getSelectedTitle();
                        lastTabs.add(new TabInfo(url, title, favicon));
                    }
                }.parse(jsonString);

                return lastTabs;
            }

            @Override
            public void onPostExecute(final ArrayList<TabInfo> lastTabs) {
                if (lastTabs == null) {
                    return;
                }

                for (TabInfo tab : lastTabs) {
                    final View tabView = createTabView(tab, getItemsContainer());
                    addItem(tabView);
                }

                // If we have at least one tab from last time, show the
                // container view.
                final int numTabs = lastTabs.size();
                if (numTabs > 1) {
                    // If we have more than one tab from last time, show the
                    // "Open all tabs" button
                    showMoreText();
                    setOnMoreTextClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            int flags = Tabs.LOADURL_NEW_TAB;
                            if (Tabs.getInstance().getSelectedTab().isPrivate())
                                flags |= Tabs.LOADURL_PRIVATE;
                            for (TabInfo tab : lastTabs) {
                                Tabs.getInstance().loadUrl(tab.url, flags);
                            }
                        }
                    });
                    show();
                } else if (numTabs == 1) {
                    hideMoreText();
                    show();
                }
            }
        }.execute();
    }

    View createTabView(final TabInfo tab, ViewGroup parent) {
        final String url = tab.url;
        final Bitmap favicon = tab.favicon;

        View tabView = LayoutInflater.from(mContext).inflate(R.layout.abouthome_last_tabs_row, parent, false);
        ((TextView) tabView.findViewById(R.id.last_tab_title)).setText(tab.title);
        ((TextView) tabView.findViewById(R.id.last_tab_url)).setText(url);
        if (favicon != null) {
            FaviconView faviconView = (FaviconView) tabView.findViewById(R.id.last_tab_favicon);
            Bitmap bitmap = Favicons.getInstance().scaleImage(favicon);
            faviconView.updateImage(favicon, url);
        }

        tabView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int flags = Tabs.LOADURL_NEW_TAB;
                if (Tabs.getInstance().getSelectedTab().isPrivate())
                    flags |= Tabs.LOADURL_PRIVATE;
                Tabs.getInstance().loadUrl(url, flags);
            }
        });
        tabView.setOnKeyListener(GamepadUtils.getClickDispatcher());

        return tabView;
    }
}
