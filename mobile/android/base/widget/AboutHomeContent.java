/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.LightweightThemeDrawable;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.database.ContentObserver;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.widget.ScrollView;

import java.util.EnumSet;

public class AboutHomeContent extends ScrollView
                              implements LightweightTheme.OnChangeListener {
    public static enum UpdateFlags {
        TOP_SITES,
        PREVIOUS_TABS,
        RECOMMENDED_ADDONS,
        REMOTE_TABS;

        public static final EnumSet<UpdateFlags> ALL = EnumSet.allOf(UpdateFlags.class);
    }

    private Context mContext;
    private BrowserApp mActivity;
    private UriLoadCallback mUriLoadCallback = null;

    private ContentObserver mTabsContentObserver = null;

    protected TopSitesView mTopSites;
    protected AddonsSection mAddons;
    protected LastTabsSection mLastTabs;
    protected RemoteTabsSection mRemoteTabs;
    private PromoBox mPromoBox;

    public interface UriLoadCallback {
        public void callback(String uriSpec);
    }

    public interface VoidCallback {
        public void callback();
    }

    public AboutHomeContent(Context context) {
        super(context);
        mContext = context;
        mActivity = (BrowserApp) context;
    }

    public AboutHomeContent(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (BrowserApp) context;
    }

    public void init() {
        inflate();

        // Reload the mobile homepage on inbound tab syncs
        // Because the tabs URI is coarse grained, this updates the
        // remote tabs component on *every* tab change
        // The observer will run on the background thread (see constructor argument)
        mTabsContentObserver = new ContentObserver(ThreadUtils.getBackgroundHandler()) {
            @Override
            public void onChange(boolean selfChange) {
                update(EnumSet.of(AboutHomeContent.UpdateFlags.REMOTE_TABS));
            }
        };
        mActivity.getContentResolver().registerContentObserver(BrowserContract.Tabs.CONTENT_URI,
                false, mTabsContentObserver);
    }

    private void inflate() {
        LayoutInflater.from(mContext).inflate(R.layout.abouthome_content, this);

        mTopSites = (TopSitesView) findViewById(R.id.top_sites_grid);
        mPromoBox = (PromoBox) findViewById(R.id.promo_box);
        mAddons = (AddonsSection) findViewById(R.id.recommended_addons);
        mLastTabs = (LastTabsSection) findViewById(R.id.last_tabs);
        mRemoteTabs = (RemoteTabsSection) findViewById(R.id.remote_tabs);

        // Inform the new views about the UriLoadCallback
        if (mUriLoadCallback != null)
            setUriLoadCallback(mUriLoadCallback);

        mPromoBox.showRandomPromo();
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mActivity.getLightweightTheme().addListener(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mActivity.getLightweightTheme().removeListener(this);
    }

    public void onDestroy() {
        if (mTopSites != null) {
            mTopSites.onDestroy();
        }

        if (mTabsContentObserver != null) {
            mActivity.getContentResolver().unregisterContentObserver(mTabsContentObserver);
            mTabsContentObserver = null;
        }
    }

    private void loadTopSites() {
        mTopSites.loadTopSites();
    }

    public void openNewTab(ContextMenuInfo info) {
        mTopSites.openNewTab(info);
    }

    public void openNewPrivateTab(ContextMenuInfo info) {
        mTopSites.openNewPrivateTab(info);
    }

    public void pinSite(ContextMenuInfo info) {
        mTopSites.pinSite(info);
    }

    public void unpinSite(ContextMenuInfo info, final TopSitesView.UnpinFlags flags) {
        mTopSites.unpinSite(info, flags);
    }

    public void editSite(ContextMenuInfo info) {
        mTopSites.editSite(info);
    }

    public void update(final EnumSet<UpdateFlags> flags) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                if (flags.contains(UpdateFlags.TOP_SITES))
                    loadTopSites();

                if (flags.contains(UpdateFlags.PREVIOUS_TABS))
                    readLastTabs();

                if (flags.contains(UpdateFlags.RECOMMENDED_ADDONS))
                    readRecommendedAddons();

                if (flags.contains(UpdateFlags.REMOTE_TABS))
                    loadRemoteTabs();
            }
        });
    }

    public void setUriLoadCallback(UriLoadCallback uriLoadCallback) {
        mUriLoadCallback = uriLoadCallback;
        mTopSites.setUriLoadCallback(uriLoadCallback);
        mAddons.setUriLoadCallback(uriLoadCallback);
    }

    public void setLoadCompleteCallback(VoidCallback callback) {
        if (mTopSites != null)
            mTopSites.setLoadCompleteCallback(callback);
    }

    public void onActivityContentChanged() {
        update(EnumSet.of(UpdateFlags.TOP_SITES));
    }

    /**
     * Reinflates and updates all components of this view.
     */
    public void refresh() {
        mTopSites.onDestroy();

        // We must remove the currently inflated view to allow for reinflation.
        removeAllViews();

        inflate();

        // Refresh all elements.
        update(AboutHomeContent.UpdateFlags.ALL);
    }

    public void setLastTabsVisibility(boolean visible) {
        if (visible)
            mLastTabs.show();
        else
            mLastTabs.hide();
    }

    private void readLastTabs() {
        mLastTabs.readLastTabs(mActivity.getProfile());
    }

    private void loadRemoteTabs() {
        mRemoteTabs.loadRemoteTabs();
    }

    private void readRecommendedAddons() {
        mAddons.readRecommendedAddons();
    }

    @Override
    public void onLightweightThemeChanged() {
        LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getColorDrawable(this);
        if (drawable == null)
            return;

         drawable.setAlpha(255, 0);
         setBackgroundDrawable(drawable);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundColor(getContext().getResources().getColor(R.color.background_normal));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        onLightweightThemeChanged();
    }
}
