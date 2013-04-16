/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.TabsAccessor;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class RemoteTabsSection extends AboutHomeSection
                               implements TabsAccessor.OnQueryTabsCompleteListener {
    private static final int NUMBER_OF_REMOTE_TABS = 5;

    private Context mContext;
    private BrowserApp mActivity;

    private View.OnClickListener mRemoteTabClickListener;

    public RemoteTabsSection(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (BrowserApp) context;

        setOnMoreTextClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mActivity.showRemoteTabs();
            }
        });

        mRemoteTabClickListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int flags = Tabs.LOADURL_NEW_TAB;
                if (Tabs.getInstance().getSelectedTab().isPrivate())
                    flags |= Tabs.LOADURL_PRIVATE;
                Tabs.getInstance().loadUrl((String) v.getTag(), flags);
            }
        };
    }

    public void loadRemoteTabs() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                if (!SyncAccounts.syncAccountsExist(mActivity)) {
                    post(new Runnable() {
                        @Override
                        public void run() {
                            hide();
                        }
                    });
                    return;
                }

                TabsAccessor.getTabs(getContext(), NUMBER_OF_REMOTE_TABS, RemoteTabsSection.this);
            }
        });
    }

    @Override
    public void onQueryTabsComplete(List<TabsAccessor.RemoteTab> tabsList) {
        ArrayList<TabsAccessor.RemoteTab> tabs = new ArrayList<TabsAccessor.RemoteTab> (tabsList);
        if (tabs == null || tabs.size() == 0) {
            hide();
            return;
        }

        clear();

        String client = null;

        for (TabsAccessor.RemoteTab tab : tabs) {
            if (client == null)
                client = tab.name;
            else if (!TextUtils.equals(client, tab.name))
                break;

            final TextView row = (TextView) LayoutInflater.from(mContext).inflate(R.layout.abouthome_remote_tab_row, getItemsContainer(), false);
            row.setText(TextUtils.isEmpty(tab.title) ? tab.url : tab.title);
            row.setTag(tab.url);
            addItem(row);
            row.setOnClickListener(mRemoteTabClickListener);
            row.setOnKeyListener(GamepadUtils.getClickDispatcher());
        }

        setSubtitle(client);
        show();
    }
}
