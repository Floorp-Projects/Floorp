/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import java.util.EnumSet;

import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;

import android.app.Activity;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

public class AboutHome extends Fragment {
    public static enum UpdateFlags {
        TOP_SITES,
        PREVIOUS_TABS,
        RECOMMENDED_ADDONS,
        REMOTE_TABS;

        public static final EnumSet<UpdateFlags> ALL = EnumSet.allOf(UpdateFlags.class);
    }

    private UriLoadListener mUriLoadListener;
    private LoadCompleteListener mLoadCompleteListener;
    private LightweightTheme mLightweightTheme;
    private ContentObserver mTabsContentObserver;
    private int mTopPadding;
    private AboutHomeView mAboutHomeView;
    private AddonsSection mAddonsSection;
    private LastTabsSection mLastTabsSection;
    private RemoteTabsSection mRemoteTabsSection;
    private TopSitesView mTopSitesView;

    private static final String STATE_TOP_PADDING = "top_padding";

    public interface UriLoadListener {
        public void onAboutHomeUriLoad(String uriSpec);
    }

    public interface LoadCompleteListener {
        public void onAboutHomeLoadComplete();
    }

    public static AboutHome newInstance() {
        return new AboutHome();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mLightweightTheme = ((GeckoApplication) getActivity().getApplication()).getLightweightTheme();

        if (savedInstanceState != null) {
            mTopPadding = savedInstanceState.getInt(STATE_TOP_PADDING, 0);
        }
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUriLoadListener = (UriLoadListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement UriLoadListener");
        }

        try {
            mLoadCompleteListener = (LoadCompleteListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement LoadCompleteListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUriLoadListener = null;
        mLoadCompleteListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        super.onCreateView(inflater, container, savedInstanceState);

        mAboutHomeView = (AboutHomeView) inflater.inflate(R.layout.abouthome_content, container, false);
        mAddonsSection = (AddonsSection) mAboutHomeView.findViewById(R.id.recommended_addons);
        mLastTabsSection = (LastTabsSection) mAboutHomeView.findViewById(R.id.last_tabs);
        mRemoteTabsSection = (RemoteTabsSection) mAboutHomeView.findViewById(R.id.remote_tabs);
        mTopSitesView = (TopSitesView) mAboutHomeView.findViewById(R.id.top_sites_grid);

        mAboutHomeView.setLightweightTheme(mLightweightTheme);
        mLightweightTheme.addListener(mAboutHomeView);

        return mAboutHomeView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        view.setPadding(0, mTopPadding, 0, 0);
        ((PromoBox) view.findViewById(R.id.promo_box)).showRandomPromo();
        update(AboutHome.UpdateFlags.ALL);

        mTopSitesView.setLoadCompleteListener(mLoadCompleteListener);
        mTopSitesView.setUriLoadListener(mUriLoadListener);
        mAddonsSection.setUriLoadListener(mUriLoadListener);

        // Reload the mobile homepage on inbound tab syncs
        // Because the tabs URI is coarse grained, this updates the
        // remote tabs component on *every* tab change
        mTabsContentObserver = new ContentObserver(null) {
            @Override
            public void onChange(boolean selfChange) {
                update(EnumSet.of(AboutHome.UpdateFlags.REMOTE_TABS));
            }
        };
        getActivity().getContentResolver().registerContentObserver(BrowserContract.Tabs.CONTENT_URI,
                false, mTabsContentObserver);
    }

    @Override
    public void onDestroyView() {
        mLightweightTheme.removeListener(mAboutHomeView);
        getActivity().getContentResolver().unregisterContentObserver(mTabsContentObserver);
        mTopSitesView.onDestroy();

        mAboutHomeView = null;
        mAddonsSection = null;
        mLastTabsSection = null;
        mRemoteTabsSection = null;
        mTopSitesView = null;

        super.onDestroyView();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Reattach the fragment, forcing a reinflation of its view.
        // We use commitAllowingStateLoss() instead of commit() here to avoid
        // an IllegalStateException. If the phone is rotated while Fennec
        // is in the background, onConfigurationChanged() is fired.
        // onConfigurationChanged() is called before onResume(), so
        // using commit() would throw an IllegalStateException since it can't
        // be used between the Activity's onSaveInstanceState() and
        // onResume().
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        ContextMenuInfo info = item.getMenuInfo();

        if (getView() == null) {
            return true;
        }

        switch (item.getItemId()) {
            case R.id.abouthome_open_new_tab:
                mTopSitesView.openNewTab(info);
                return true;

            case R.id.abouthome_open_private_tab:
                mTopSitesView.openNewPrivateTab(info);
                return true;

            case R.id.abouthome_topsites_edit:
                mTopSitesView.editSite(info);
                return true;

            case R.id.abouthome_topsites_unpin:
                mTopSitesView.unpinSite(info, TopSitesView.UnpinFlags.REMOVE_PIN);
                return true;

            case R.id.abouthome_topsites_pin:
                mTopSitesView.pinSite(info);
                return true;

        }
        return super.onContextItemSelected(item);
    }

    public void update(final EnumSet<UpdateFlags> flags) {
        if (getView() == null) {
            return;
        }

        if (flags.contains(UpdateFlags.TOP_SITES)) {
            mTopSitesView.loadTopSites();
        }

        if (flags.contains(UpdateFlags.PREVIOUS_TABS)) {
            mLastTabsSection.readLastTabs();
        }

        if (flags.contains(UpdateFlags.RECOMMENDED_ADDONS)) {
            mAddonsSection.readRecommendedAddons();
        }

        if (flags.contains(UpdateFlags.REMOTE_TABS)) {
            mRemoteTabsSection.loadRemoteTabs();
        }
    }

    public void setLastTabsVisibility(boolean visible) {
        if (mAboutHomeView == null) {
            return;
        }

        if (visible)
            mLastTabsSection.show();
        else
            mLastTabsSection.hide();
    }

    public void requestFocus() {
        View view = getView();
        if (view != null) {
            view.requestFocus();
        }
    }

    public void setTopPadding(int topPadding) {
        View view = getView();
        if (view != null) {
            view.setPadding(0, topPadding, 0, 0);
        }

        // If the padding has changed but the view hasn't been created yet,
        // store the padding values here; they will be used later in
        // onViewCreated().
        mTopPadding = topPadding;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(STATE_TOP_PADDING, mTopPadding);
    }
}
