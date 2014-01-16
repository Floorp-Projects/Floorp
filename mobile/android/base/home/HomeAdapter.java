/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.home.HomePager;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentStatePagerAdapter;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

class HomeAdapter extends FragmentStatePagerAdapter {

    private final Context mContext;
    private final ArrayList<PanelInfo> mPanelInfos;
    private final HashMap<String, Fragment> mPanels;

    private boolean mCanLoadHint;

    private OnAddPanelListener mAddPanelListener;

    interface OnAddPanelListener {
        public void onAddPanel(String title);
    }

    public HomeAdapter(Context context, FragmentManager fm) {
        super(fm);

        mContext = context;
        mCanLoadHint = HomeFragment.DEFAULT_CAN_LOAD_HINT;

        mPanelInfos = new ArrayList<PanelInfo>();
        mPanels = new HashMap<String, Fragment>();
    }

    @Override
    public int getCount() {
        return mPanelInfos.size();
    }

    @Override
    public Fragment getItem(int position) {
        PanelInfo info = mPanelInfos.get(position);
        return Fragment.instantiate(mContext, info.getClassName(), info.getArgs());
    }

    @Override
    public CharSequence getPageTitle(int position) {
        if (mPanelInfos.size() > 0) {
            PanelInfo info = mPanelInfos.get(position);
            return info.getTitle().toUpperCase();
        }

        return null;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        Fragment fragment = (Fragment) super.instantiateItem(container, position);
        mPanels.put(mPanelInfos.get(position).getId(), fragment);

        return fragment;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        super.destroyItem(container, position, object);
        mPanels.remove(mPanelInfos.get(position).getId());
    }

    public void setOnAddPanelListener(OnAddPanelListener listener) {
        mAddPanelListener = listener;
    }

    public int getItemPosition(String panelId) {
        for (int i = 0; i < mPanelInfos.size(); i++) {
            final String id = mPanelInfos.get(i).getId();
            if (id.equals(panelId)) {
                return i;
            }
        }

        return -1;
    }

    public String getPanelIdAtPosition(int position) {
        // getPanelIdAtPosition() might be called before HomeAdapter
        // has got its initial list of PanelConfigs. Just bail.
        if (mPanelInfos.isEmpty()) {
            return null;
        }

        return mPanelInfos.get(position).getId();
    }

    private void addPanel(PanelInfo info) {
        mPanelInfos.add(info);

        if (mAddPanelListener != null) {
            mAddPanelListener.onAddPanel(info.getTitle());
        }
    }

    public void update(List<PanelConfig> panelConfigs) {
        mPanels.clear();
        mPanelInfos.clear();

        if (panelConfigs != null) {
            for (PanelConfig panelConfig : panelConfigs) {
                final PanelInfo info = new PanelInfo(panelConfig);
                addPanel(info);
            }
        }

        notifyDataSetChanged();
    }

    public boolean getCanLoadHint() {
        return mCanLoadHint;
    }

    public void setCanLoadHint(boolean canLoadHint) {
        // We cache the last hint value so that we can use it when
        // creating new panels. See PanelInfo.getArgs().
        mCanLoadHint = canLoadHint;

        // Enable/disable loading on all existing panels
        for (Fragment panelFragment : mPanels.values()) {
            final HomeFragment panel = (HomeFragment) panelFragment;
            panel.setCanLoadHint(canLoadHint);
        }
    }

    private final class PanelInfo {
        private final PanelConfig mPanelConfig;

        PanelInfo(PanelConfig panelConfig) {
            mPanelConfig = panelConfig;
        }

        public String getId() {
            return mPanelConfig.getId();
        }

        public String getTitle() {
            return mPanelConfig.getTitle();
        }

        public String getClassName() {
            final PanelType type = mPanelConfig.getType();
            return type.getPanelClass().getName();
        }

        public Bundle getArgs() {
            final Bundle args = new Bundle();

            args.putBoolean(HomePager.CAN_LOAD_ARG, mCanLoadHint);

            // Only DynamicPanels need the PanelConfig argument
            if (mPanelConfig.getType() == PanelType.DYNAMIC) {
                args.putParcelable(HomePager.PANEL_CONFIG_ARG, mPanelConfig);
            }

            return args;
        }
    }
}
