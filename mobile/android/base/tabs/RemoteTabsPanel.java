/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.tabs.TabsPanel.PanelView;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

/**
 * A tabs tray panel that displays a static view that informs users that the
 * Synced Tabs/Remote Tabs list is now accessed as a home panel. The view
 * provides a single link that opens the new home panel.
 */
class RemoteTabsPanel extends FrameLayout implements PanelView {
    private TabsPanel tabsPanel;

    public RemoteTabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.remote_tabs_panel, this);

        final View link = findViewById(R.id.go_to_panel);
        link.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                // For now, we don't do anything.
                if (tabsPanel != null) {
                    tabsPanel.autoHidePanel();
                }
            }
        });
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
    }

    @Override
    public void show() {
        setVisibility(View.VISIBLE);
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
    }

    @Override
    public boolean shouldExpand() {
        final LinearLayout container = (LinearLayout) findViewById(R.id.container);
        return container.getOrientation() == LinearLayout.VERTICAL;
    }
}
