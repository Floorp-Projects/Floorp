/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabspanel;

import org.mozilla.gecko.R;
import org.mozilla.gecko.TabsAccessor;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout;

import android.accounts.Account;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

/**
 * Provides a container to wrap the list of synced tabs and provide swipe-to-refresh support. The
 * only child view should be an instance of {@link RemoteTabsList}.
 */
public class RemoteTabsContainer extends GeckoSwipeRefreshLayout
                                 implements TabsPanel.PanelView {
    private static final String[] STAGES_TO_SYNC_ON_REFRESH = new String[] { "tabs" };

    private final Context context;
    private final RemoteTabsSyncObserver syncListener;
    private RemoteTabsList list;

    public RemoteTabsContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.context = context;
        this.syncListener = new RemoteTabsSyncObserver();

        setOnRefreshListener(new RemoteTabsRefreshListener());
    }

    @Override
    public void addView(View child, int index, ViewGroup.LayoutParams params) {
        super.addView(child, index, params);

        list = (RemoteTabsList) child;

        // Must be called after the child view has been added.
        setColorScheme(R.color.swipe_refresh_orange_dark, R.color.background_tabs,
                       R.color.swipe_refresh_orange_dark, R.color.background_tabs);
    }


    @Override
    public boolean canChildScrollUp() {
        // We are not supporting swipe-to-refresh for old sync. This disables the swipe gesture if
        // no FxA are detected.
        if (FirefoxAccounts.firefoxAccountsExist(getContext())) {
            return super.canChildScrollUp();
        } else {
            return true;
        }
    }

    @Override
    public ViewGroup getLayout() {
        return this;
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        list.setTabsPanel(panel);
    }

    @Override
    public void show() {
        setVisibility(VISIBLE);
        TabsAccessor.getTabs(context, list);
        FirefoxAccounts.addSyncStatusListener(syncListener);
    }

    @Override
    public void hide() {
        setVisibility(GONE);
        FirefoxAccounts.removeSyncStatusListener(syncListener);
    }

    @Override
    public boolean shouldExpand() {
        return true;
    }

    private class RemoteTabsRefreshListener implements OnRefreshListener {
        @Override
        public void onRefresh() {
            if (FirefoxAccounts.firefoxAccountsExist(getContext())) {
                final Account account = FirefoxAccounts.getFirefoxAccount(getContext());
                FirefoxAccounts.requestSync(account, FirefoxAccounts.FORCE, STAGES_TO_SYNC_ON_REFRESH, null);
            }
        }
    }

    private class RemoteTabsSyncObserver implements FirefoxAccounts.SyncStatusListener {
        @Override
        public Context getContext() {
            return RemoteTabsContainer.this.getContext();
        }

        @Override
        public Account getAccount() {
            return FirefoxAccounts.getFirefoxAccount(getContext());
        }

        public void onSyncFinished() {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    TabsAccessor.getTabs(context, list);
                    setRefreshing(false);
                }
            });
        }

        public void onSyncStarted() {}
    }
}
