/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.TabsAccessor;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.tabs.TabsPanel.Panel;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout;

import android.accounts.Account;
import android.content.Context;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

/**
 * Provides a container to wrap the list of synced tabs and provide
 * swipe-to-refresh support. The only child view should be an instance of
 * {@link RemoteTabsList}.
 */
public class RemoteTabsContainerPanel extends GeckoSwipeRefreshLayout
                                 implements TabsPanel.PanelView {
    private static final String[] STAGES_TO_SYNC_ON_REFRESH = new String[] { "clients", "tabs" };

    /**
     * Refresh indicators (the swipe-to-refresh "laser show" and the spinning
     * icon) will never be shown for less than the following duration, in
     * milliseconds.
     */
    private static final long MINIMUM_REFRESH_INDICATOR_DURATION_IN_MS = 12 * 100; // 12 frames, 100 ms each.

    private final Context context;
    private final RemoteTabsSyncObserver syncListener;
    private TabsPanel panel;
    private RemoteTabsList list;

    // Whether or not a sync status listener is attached.
    private boolean isListening;

    public RemoteTabsContainerPanel(Context context, AttributeSet attrs) {
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
        setColorScheme(R.color.swipe_refresh_orange1, R.color.swipe_refresh_orange2,
                       R.color.swipe_refresh_orange3, R.color.swipe_refresh_orange4);
    }


    @Override
    public boolean canChildScrollUp() {
        // We are not supporting swipe-to-refresh for old sync. This disables
        // the swipe gesture if no FxA are detected.
        if (FirefoxAccounts.firefoxAccountsExist(getContext())) {
            return super.canChildScrollUp();
        } else {
            return true;
        }
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        this.panel = panel;
        list.setTabsPanel(panel);
    }

    @Override
    public void show() {
        // Start fetching remote tabs.
        TabsAccessor.getTabs(context, list);
        // The user can trigger a tabs sync, so we want to be very certain the
        // locally-persisted tabs are fresh (tab writes are batched and delayed).
        Tabs.getInstance().persistAllTabs();

        if (!isListening) {
            isListening = true;
            FirefoxAccounts.addSyncStatusListener(syncListener);
        }
        setVisibility(View.VISIBLE);
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        if (isListening) {
            isListening = false;
            FirefoxAccounts.removeSyncStatusListener(syncListener);
        }
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
        // Written on the main thread, and read off the main thread, but no need
        // to synchronize.
        protected volatile long lastSyncStarted;

        @Override
        public Context getContext() {
            return RemoteTabsContainerPanel.this.getContext();
        }

        @Override
        public Account getAccount() {
            return FirefoxAccounts.getFirefoxAccount(getContext());
        }

        public void onSyncStarted() {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    lastSyncStarted = System.currentTimeMillis();

                    // Replace the static sync icon with an animated icon, and
                    // start the animation. This works around an Android 4.4.2
                    // bug, which makes animating the animated icon unreliable.
                    // See Bug 1015974.
                    panel.setIconDrawable(Panel.REMOTE_TABS, R.drawable.tabs_synced_animated);
                    final Drawable iconDrawable = panel.getIconDrawable(Panel.REMOTE_TABS);
                    if (iconDrawable instanceof AnimationDrawable) {
                        ((AnimationDrawable) iconDrawable).start();
                    }
                }
            });
        }

        public void onSyncFinished() {
            final Handler uiHandler = ThreadUtils.getUiHandler();

            // We want to update the list immediately ...
            uiHandler.post(new Runnable() {
                @Override
                public void run() {
                    TabsAccessor.getTabs(context, list);
                }
            });

            // ... but we want the refresh indicators to persist for long enough
            // to be visible.
            final long last = lastSyncStarted;
            final long now = System.currentTimeMillis();
            final long delay = Math.max(0, MINIMUM_REFRESH_INDICATOR_DURATION_IN_MS - (now - last));

            uiHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    setRefreshing(false);

                    // Replace the animated sync icon with the static sync icon.
                    // This works around an Android 4.4.2 bug, which makes
                    // animating the animated icon unreliable. See Bug 1015974.
                    panel.setIconDrawable(Panel.REMOTE_TABS, R.drawable.tabs_synced);
                }
            }, delay);
        }
    }
}
