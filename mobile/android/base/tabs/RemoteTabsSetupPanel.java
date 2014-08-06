/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.Locale;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.activities.FxAccountCreateAccountActivity;
import org.mozilla.gecko.tabs.TabsPanel.PanelView;

import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

/**
 * A tabs panel which allows a user to get started setting up a Firefox
 * Accounts account. Currently used as one sub-panel in a sequence
 * contained by the {@link RemoteTabsPanel}.
 */
class RemoteTabsSetupPanel extends ScrollView implements PanelView {
    private final LinearLayout containingLayout;

    private TabsPanel tabsPanel;

    public RemoteTabsSetupPanel(Context context) {
        super(context);

        LayoutInflater.from(context).inflate(R.layout.remote_tabs_setup_panel, this);
        containingLayout = (LinearLayout) findViewById(R.id.remote_tabs_setup_containing_layout);

        final View setupGetStartedButton =
                containingLayout.findViewById(R.id.remote_tabs_setup_get_started);
        setupGetStartedButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(final View v) {
                final Context context = getContext();
                // This Activity will redirect to the correct Activity if the
                // account is no longer in the setup state.
                final Intent intent = new Intent(context, FxAccountCreateAccountActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
        });

        final View setupOlderVersionLink =
                containingLayout.findViewById(R.id.remote_tabs_setup_old_sync_link);
        setupOlderVersionLink.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(final View v) {
                final String url = FirefoxAccounts.getOldSyncUpgradeURL(
                        getResources(), Locale.getDefault());
                Tabs.getInstance().loadUrlInTab(url);
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
        return containingLayout.getOrientation() == LinearLayout.VERTICAL;
    }
}
