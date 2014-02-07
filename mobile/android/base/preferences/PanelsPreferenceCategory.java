/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

public class PanelsPreferenceCategory extends CustomListCategory {
    public static final String LOGTAG = "PanelsPrefCategory";

    protected HomeConfig mHomeConfig;
    protected List<PanelConfig> mPanelConfigs;

    protected UiAsyncTask<Void, Void, List<PanelConfig>> mLoadTask;
    protected UiAsyncTask<Void, Void, Void> mSaveTask;

    public PanelsPreferenceCategory(Context context) {
        super(context);
        initConfig(context);
    }

    public PanelsPreferenceCategory(Context context, AttributeSet attrs) {
        super(context, attrs);
        initConfig(context);
    }

    public PanelsPreferenceCategory(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        initConfig(context);
    }

    protected void initConfig(Context context) {
        mHomeConfig = HomeConfig.getDefault(context);
    }

    @Override
    public void onAttachedToActivity() {
        super.onAttachedToActivity();

        loadHomeConfig();
    }

    /**
     * Load the Home Panels config and populate the preferences screen and maintain local state.
     */
    private void loadHomeConfig() {
        mLoadTask = new UiAsyncTask<Void, Void, List<PanelConfig>>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public List<PanelConfig> doInBackground(Void... params) {
                return mHomeConfig.load();
            }

            @Override
            public void onPostExecute(List<PanelConfig> panelConfigs) {
                mPanelConfigs = panelConfigs;
                displayHomeConfig();
            }
        };
        mLoadTask.execute();
    }

    private void displayHomeConfig() {
        for (PanelConfig panelConfig : mPanelConfigs) {
            // Create and add the pref.
            final PanelsPreference pref = new PanelsPreference(getContext(), PanelsPreferenceCategory.this);
            pref.setTitle(panelConfig.getTitle());
            pref.setKey(panelConfig.getId());
            // XXX: Pull icon from PanelInfo.
            addPreference(pref);

            if (panelConfig.isDefault()) {
                mDefaultReference = pref;
                pref.setIsDefault(true);
            }

            if (panelConfig.isDisabled()) {
                pref.setHidden(true);
            }
        }
    }

    /**
     * Update HomeConfig off the main thread.
     *
     * @param panelConfigs Configuration to be saved
     */
    private void saveHomeConfig() {
        if (mPanelConfigs == null) {
            return;
        }

        final List<PanelConfig> panelConfigs = makeConfigListDeepCopy();
        mSaveTask = new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public Void doInBackground(Void... params) {
                mHomeConfig.save(panelConfigs);
                return null;
            }
        };
        mSaveTask.execute();
    }

    private List<PanelConfig> makeConfigListDeepCopy() {
        List<PanelConfig> copiedList = new ArrayList<PanelConfig>();
        for (PanelConfig panelConfig : mPanelConfigs) {
            copiedList.add(new PanelConfig(panelConfig));
        }
        return copiedList;
    }

    @Override
    public void setDefault(CustomListPreference pref) {
        super.setDefault(pref);
        updateConfigDefault();
        saveHomeConfig();
    }

    @Override
    protected void onPrepareForRemoval() {
        if (mLoadTask != null) {
            mLoadTask.cancel(true);
        }

        if (mSaveTask != null) {
            mSaveTask.cancel(true);
        }
     }

    /**
     * Update the local HomeConfig default state from mDefaultReference.
     */
    private void updateConfigDefault() {
        String id = null;
        if (mDefaultReference != null) {
            id = mDefaultReference.getKey();
        }

        for (PanelConfig panelConfig : mPanelConfigs) {
            if (TextUtils.equals(panelConfig.getId(), id)) {
                panelConfig.setIsDefault(true);
                panelConfig.setIsDisabled(false);
            } else {
                panelConfig.setIsDefault(false);
            }
        }
    }

    @Override
    public void uninstall(CustomListPreference pref) {
        super.uninstall(pref);
        // This could change the default, so update the local version of the config.
        updateConfigDefault();

        final String id = pref.getKey();
        PanelConfig toRemove = null;
        for (PanelConfig panelConfig : mPanelConfigs) {
            if (TextUtils.equals(panelConfig.getId(), id)) {
                toRemove = panelConfig;
                break;
            }
        }
        mPanelConfigs.remove(toRemove);

        saveHomeConfig();
    }

    /**
     * Update the hide/show state of the preference and save the HomeConfig
     * changes.
     *
     * @param pref Preference to update
     * @param toHide New hidden state of the preference
     */
    protected void setHidden(PanelsPreference pref, boolean toHide) {
        pref.setHidden(toHide);
        ensureDefaultForHide(pref, toHide);

        final String id = pref.getKey();
        for (PanelConfig panelConfig : mPanelConfigs) {
            if (TextUtils.equals(panelConfig.getId(), id)) {
                panelConfig.setIsDisabled(toHide);
                break;
            }
        }

        saveHomeConfig();
    }

    /**
     * Ensure a default is set (if possible) for hiding/showing a pref.
     * If hiding, try to find an enabled pref to set as the default.
     * If showing, set it as the default if there is no default currently.
     *
     * This updates the local HomeConfig state.
     *
     * @param pref Preference getting updated
     * @param toHide Boolean of the new hidden state
     */
    private void ensureDefaultForHide(PanelsPreference pref, boolean toHide) {
        if (toHide) {
            // Set a default if there is an enabled panel left.
            if (pref == mDefaultReference) {
                setFallbackDefault();
                updateConfigDefault();
            }
        } else {
            if (mDefaultReference == null) {
                super.setDefault(pref);
                updateConfigDefault();
            }
        }
    }

    /**
     * When the default panel is removed or disabled, find an enabled panel
     * if possible and set it as mDefaultReference.
     */
    @Override
    protected void setFallbackDefault() {
        for (int i = 0; i < getPreferenceCount(); i++) {
            final PanelsPreference pref = (PanelsPreference) getPreference(i);
            if (!pref.isHidden()) {
                super.setDefault(pref);
                return;
            }
        }
        mDefaultReference = null;
    }
}
