/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.util.AttributeSet;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;

import org.mozilla.gecko.sync.setup.SyncAccounts;

public class TabsPanel extends LinearLayout {
    private static final String LOGTAG = "GeckoTabsPanel";

    public static enum Panel {
        LOCAL_TABS,
        REMOTE_TABS
    }

    public static interface PanelView {
        public ViewGroup getLayout();
        public void setHeightRestriction(boolean isRestricted);
        public void show();
        public void hide();
    }

    public static interface TabsLayoutChangeListener {
        public void onTabsLayoutChange(int width, int height);
    }

    private Context mContext;
    private PanelView mPanel;
    private TabsLayoutChangeListener mLayoutChangeListener;

    private static ImageButton mRemoteTabs;
    private TextView mTitle;

    private Panel mCurrentPanel;
    private boolean mVisible;
    private boolean mHeightRestricted;

    private static final int REMOTE_TABS_HIDDEN = 1;
    private static final int REMOTE_TABS_SHOWN = 2;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        setOrientation(LinearLayout.VERTICAL);
        LayoutInflater.from(context).inflate(R.layout.tabs_panel, this);

        mCurrentPanel = Panel.LOCAL_TABS;
        mVisible = false;
        mHeightRestricted = GeckoApp.mAppContext.isTablet() ? false : true;

        mTitle = (TextView) findViewById(R.id.title);
        ImageButton addTab = (ImageButton) findViewById(R.id.add_tab);
        addTab.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.addTab();
                hide();
            }
        });

        mRemoteTabs = (ImageButton) findViewById(R.id.remote_tabs);
        mRemoteTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                if (mRemoteTabs.getDrawable().getLevel() == REMOTE_TABS_SHOWN)
                    GeckoApp.mAppContext.showLocalTabs();
                else
                    GeckoApp.mAppContext.showRemoteTabs();
            }
        });
    }

    public void show(Panel panel) {
        if (mPanel != null) {
            // Remove the old panel.
            mPanel.hide();
            if (getChildCount() == 2)
                removeViewAt(1);
        }

        mVisible = true;
        mCurrentPanel = panel;

        if (panel == Panel.LOCAL_TABS) {
            mPanel = new TabsTray(mContext, null);
            mTitle.setText("");
            mRemoteTabs.setImageLevel(REMOTE_TABS_HIDDEN);
        } else {
            mPanel = new RemoteTabs(mContext, null);
            mTitle.setText(R.string.remote_tabs);
            mRemoteTabs.setVisibility(View.VISIBLE);
            mRemoteTabs.setImageLevel(REMOTE_TABS_SHOWN);
        }

        mPanel.setHeightRestriction(mHeightRestricted);
        mPanel.show();
        addView(mPanel.getLayout(), 1);

        // Tablet has fixed sized panel, hence we need to inform when we show.
        // Phones can be informed too. But the list wouldn't have been inflated by now.
        dispatchLayoutChange(getWidth(), getHeight());

        // If Sync is set up, query the database for remote clients.
        final Context context = mContext;
        new SyncAccounts.AccountsExistTask() {
            @Override
            protected void onPostExecute(Boolean result) {
                if (!result.booleanValue()) {
                    return;
                }
                TabsAccessor.areClientsAvailable(context, new TabsAccessor.OnClientsAvailableListener() {
                    @Override
                    public void areAvailable(boolean available) {
                        final int visibility = available ? View.VISIBLE : View.GONE;
                        mRemoteTabs.setVisibility(visibility);
                    }
                });
            }
        }.execute(context);
    }

    public void hide() {
        mVisible = false;
        dispatchLayoutChange(0, 0);
    }

    @Override
    public void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        // This is used only for sliding action on phones.
        // Tablets have a fixed size pane, hence this should be done while showing.
        if (mVisible)
            dispatchLayoutChange(width, height);
    }

    public void refresh() {
        if (mVisible)
            show(mCurrentPanel);
    }

    @Override
    public boolean isShown() {
        return mVisible;
    }

    public void setTabsLayoutChangeListener(TabsLayoutChangeListener listener) {
        mLayoutChangeListener = listener;
    }

    private void dispatchLayoutChange(int width, int height) {
        if (mLayoutChangeListener != null)
            mLayoutChangeListener.onTabsLayoutChange(width, height);
    }
}
