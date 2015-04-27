/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewTreeObserver;

import org.mozilla.gecko.BrowserApp.Refreshable;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.widget.ThemedImageButton;
import org.mozilla.gecko.widget.ThemedLinearLayout;

public class TabStrip extends ThemedLinearLayout
                      implements Refreshable {
    private static final String LOGTAG = "GeckoTabStrip";

    private final TabStripView tabStripView;
    private final ThemedImageButton addTabButton;

    private final TabsListener tabsListener;

    public TabStrip(Context context) {
        this(context, null);
    }

    public TabStrip(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(HORIZONTAL);

        LayoutInflater.from(context).inflate(R.layout.tab_strip, this);
        tabStripView = (TabStripView) findViewById(R.id.tab_strip);

        addTabButton = (ThemedImageButton) findViewById(R.id.add_tab);
        addTabButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final Tabs tabs = Tabs.getInstance();
                if (isPrivateMode()) {
                    tabs.addPrivateTab();
                } else {
                    tabs.addTab();
                }
            }
        });

        getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
                @Override
                public boolean onPreDraw() {
                    getViewTreeObserver().removeOnPreDrawListener(this);

                    final Rect r = new Rect();
                    r.left = addTabButton.getRight();
                    r.right = getWidth();
                    r.top = 0;
                    r.bottom = getHeight();

                    // Redirect touch events between the 'new tab' button and the edge
                    // of the screen to the 'new tab' button.
                    setTouchDelegate(new TouchDelegate(r, addTabButton));

                    return true;
                }
            });

        tabsListener = new TabsListener();
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        Tabs.registerOnTabsChangedListener(tabsListener);
        tabStripView.refreshTabs();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        Tabs.unregisterOnTabsChangedListener(tabsListener);
        tabStripView.clearTabs();
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        addTabButton.setPrivateMode(isPrivate);
    }

    private class TabsListener implements Tabs.OnTabsChangedListener {
        @Override
        public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
            switch (msg) {
                case RESTORED:
                    tabStripView.restoreTabs();
                    break;

                case ADDED:
                    tabStripView.addTab(tab);
                    break;

                case CLOSED:
                    tabStripView.removeTab(tab);
                    break;

                case SELECTED:
                    // Update the selected position, then fall through...
                    tabStripView.selectTab(tab);
                    setPrivateMode(tab.isPrivate());
                case UNSELECTED:
                    // We just need to update the style for the unselected tab...
                case TITLE:
                case FAVICON:
                case RECORDING_CHANGE:
                    tabStripView.updateTab(tab);
                    break;
            }
        }
    }

    @Override
    public void refresh() {
        tabStripView.refresh();
    }

    @Override
    public void onLightweightThemeChanged() {
        final Drawable drawable = getTheme().getDrawable(this);
        if (drawable == null) {
            return;
        }

        final StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.text_and_tabs_tray_grey));
        stateList.addState(EMPTY_STATE_SET, drawable);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        final int defaultBackgroundColor = getResources().getColor(R.color.text_and_tabs_tray_grey);
        setBackgroundColor(defaultBackgroundColor);
    }
}
