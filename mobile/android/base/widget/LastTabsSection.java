/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SessionParser;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;

public class LastTabsSection extends AboutHomeSection {
    private Context mContext;

    public LastTabsSection(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void readLastTabs() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                String jsonString = GeckoProfile.get(mContext).readSessionFile(true);
                if (jsonString == null) {
                    // no previous session data
                    return;
                }

                final ArrayList<String> lastTabUrlsList = new ArrayList<String>();
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
                        lastTabUrlsList.add(url);

                        LastTabsSection.this.post(new Runnable() {
                            @Override
                            public void run() {
                                View container = LayoutInflater.from(mContext).inflate(R.layout.abouthome_last_tabs_row, getItemsContainer(), false);
                                ((TextView) container.findViewById(R.id.last_tab_title)).setText(tab.getSelectedTitle());
                                ((TextView) container.findViewById(R.id.last_tab_url)).setText(tab.getSelectedUrl());
                                if (favicon != null) {
                                    ((ImageView) container.findViewById(R.id.last_tab_favicon)).setImageBitmap(favicon);
                                }

                                container.setOnClickListener(new View.OnClickListener() {
                                    @Override
                                    public void onClick(View v) {
                                        int flags = Tabs.LOADURL_NEW_TAB;
                                        if (Tabs.getInstance().getSelectedTab().isPrivate())
                                            flags |= Tabs.LOADURL_PRIVATE;
                                        Tabs.getInstance().loadUrl(url, flags);
                                    }
                                });
                                container.setOnKeyListener(GamepadUtils.getClickDispatcher());

                                addItem(container);
                            }
                        });
                    }
                }.parse(jsonString);

                final int nu = lastTabUrlsList.size();
                if (nu >= 1) {
                    post(new Runnable() {
                        @Override
                        public void run() {
                            if (nu > 1) {
                                showMoreText();
                                setOnMoreTextClickListener(new View.OnClickListener() {
                                    @Override
                                    public void onClick(View v) {
                                        int flags = Tabs.LOADURL_NEW_TAB;
                                        if (Tabs.getInstance().getSelectedTab().isPrivate())
                                            flags |= Tabs.LOADURL_PRIVATE;
                                        for (String url : lastTabUrlsList) {
                                            Tabs.getInstance().loadUrl(url, flags);
                                        }
                                    }
                                });
                            } else if (nu == 1) {
                                hideMoreText();
                            }
                            show();
                        }
                    });
                }
            }
        });
    }
}
