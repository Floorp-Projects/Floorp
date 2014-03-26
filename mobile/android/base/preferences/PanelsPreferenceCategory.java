/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.Context;
import android.util.AttributeSet;

public class PanelsPreferenceCategory extends CustomListCategory {
    public static final String LOGTAG = "PanelsPrefCategory";

    protected HomeConfig mHomeConfig;
    protected HomeConfig.Editor mConfigEditor;

    protected UiAsyncTask<Void, Void, HomeConfig.State> mLoadTask;

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
        mLoadTask = new UiAsyncTask<Void, Void, HomeConfig.State>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public HomeConfig.State doInBackground(Void... params) {
                return mHomeConfig.load();
            }

            @Override
            public void onPostExecute(HomeConfig.State configState) {
                mConfigEditor = configState.edit();
                displayHomeConfig(configState);
            }
        };
        mLoadTask.execute();
    }

    /**
     * Reload the Home Panels list from HomeConfig.
     */
    public void refresh() {
        // Clear all the existing home panels, but leave the
        // first item (Add panels).
        int prefCount = getPreferenceCount();
        while (prefCount > 1) {
            removePreference(getPreference(1));
            prefCount--;
        }

        loadHomeConfig();
    }

    private void displayHomeConfig(HomeConfig.State configState) {
        for (PanelConfig panelConfig : configState) {
            final boolean isRemovable = panelConfig.isDynamic();

            // Create and add the pref.
            final PanelsPreference pref = new PanelsPreference(getContext(), PanelsPreferenceCategory.this, isRemovable);
            pref.setTitle(panelConfig.getTitle());
            pref.setKey(panelConfig.getId());
            // XXX: Pull icon from PanelInfo.
            addPreference(pref);

            if (panelConfig.isDisabled()) {
                pref.setHidden(true);
            }
        }

        setDefaultFromConfig();
    }

    private void setDefaultFromConfig() {
        final String defaultPanelId = mConfigEditor.getDefaultPanelId();
        if (defaultPanelId == null) {
            mDefaultReference = null;
            return;
        }

        final int prefCount = getPreferenceCount();

        // First preference (index 0) is Preference to add panels.
        for (int i = 1; i < prefCount; i++) {
            final PanelsPreference pref = (PanelsPreference) getPreference(i);

            if (defaultPanelId.equals(pref.getKey())) {
                super.setDefault(pref);
                break;
            }
        }
    }

    @Override
    public void setDefault(CustomListPreference pref) {
        super.setDefault(pref);

        final String id = pref.getKey();

        final String defaultPanelId = mConfigEditor.getDefaultPanelId();
        if (defaultPanelId != null && defaultPanelId.equals(id)) {
            return;
        }

        mConfigEditor.setDefault(id);
        mConfigEditor.apply();
    }

    @Override
    protected void onPrepareForRemoval() {
        if (mLoadTask != null) {
            mLoadTask.cancel(true);
        }
    }

    @Override
    public void uninstall(CustomListPreference pref) {
        mConfigEditor.uninstall(pref.getKey());
        mConfigEditor.apply();

        super.uninstall(pref);
    }

    /**
     * Update the hide/show state of the preference and save the HomeConfig
     * changes.
     *
     * @param pref Preference to update
     * @param toHide New hidden state of the preference
     */
    protected void setHidden(PanelsPreference pref, boolean toHide) {
        mConfigEditor.setDisabled(pref.getKey(), toHide);
        mConfigEditor.apply();

        pref.setHidden(toHide);
        setDefaultFromConfig();
    }

    /**
     * When the default panel is removed or disabled, find an enabled panel
     * if possible and set it as mDefaultReference.
     */
    @Override
    protected void setFallbackDefault() {
        setDefaultFromConfig();
    }
}
