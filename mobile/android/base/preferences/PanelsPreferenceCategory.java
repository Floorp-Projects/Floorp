/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.State;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.Context;
import android.text.TextUtils;
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

        loadHomeConfig(null);
    }

    /**
     * Load the Home Panels config and populate the preferences screen and maintain local state.
     */
    private void loadHomeConfig(final String animatePanelId) {
        mLoadTask = new UiAsyncTask<Void, Void, HomeConfig.State>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public HomeConfig.State doInBackground(Void... params) {
                return mHomeConfig.load();
            }

            @Override
            public void onPostExecute(HomeConfig.State configState) {
                mConfigEditor = configState.edit();
                displayHomeConfig(configState, animatePanelId);
            }
        };
        mLoadTask.execute();
    }

    /**
     * Simplified refresh of Home Panels when there is no state to be persisted.
     */
    public void refresh() {
        refresh(null, null);
    }

    /**
     * Refresh the Home Panels list and animate a panel, if specified.
     * If null, load from HomeConfig.
     *
     * @param State HomeConfig.State to rebuild Home Panels list from.
     * @param String panelId of panel to be animated.
     */
    public void refresh(State state, String animatePanelId) {
        // Clear all the existing home panels.
        removeAll();

        if (state == null) {
            loadHomeConfig(animatePanelId);
        } else {
            displayHomeConfig(state, animatePanelId);
        }
    }

    private void displayHomeConfig(HomeConfig.State configState, String animatePanelId) {
        int index = 0;
        for (PanelConfig panelConfig : configState) {
            final boolean isRemovable = panelConfig.isDynamic();

            // Create and add the pref.
            final String panelId = panelConfig.getId();
            final boolean animate = TextUtils.equals(animatePanelId, panelId);

            final PanelsPreference pref = new PanelsPreference(getContext(), PanelsPreferenceCategory.this, isRemovable, index, animate);
            pref.setTitle(panelConfig.getTitle());
            pref.setKey(panelConfig.getId());
            // XXX: Pull icon from PanelInfo.
            addPreference(pref);

            if (panelConfig.isDisabled()) {
                pref.setHidden(true);
            }

            index++;
        }

        setPositionState();
        setDefaultFromConfig();
    }

    private void setPositionState() {
        final int prefCount = getPreferenceCount();

        // Pass in position state to first and last preference.
        final PanelsPreference firstPref = (PanelsPreference) getPreference(0);
        firstPref.setIsFirst();

        final PanelsPreference lastPref = (PanelsPreference) getPreference(prefCount - 1);
        lastPref.setIsLast();
    }

    private void setDefaultFromConfig() {
        final String defaultPanelId = mConfigEditor.getDefaultPanelId();
        if (defaultPanelId == null) {
            mDefaultReference = null;
            return;
        }

        final int prefCount = getPreferenceCount();

        for (int i = 0; i < prefCount; i++) {
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

        Telemetry.sendUIEvent(TelemetryContract.Event.PANEL_SET_DEFAULT, Method.NONE, id);
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

    public void moveUp(PanelsPreference pref) {
        final int panelIndex = pref.getIndex();
        if (panelIndex > 0) {
            final String panelKey = pref.getKey();
            mConfigEditor.moveTo(panelKey, panelIndex - 1);
            final State state = mConfigEditor.apply();
            refresh(state, panelKey);
        }
    }

    public void moveDown(PanelsPreference pref) {
        final int panelIndex = pref.getIndex();
        if (panelIndex < getPreferenceCount() - 1) {
            final String panelKey = pref.getKey();
            mConfigEditor.moveTo(panelKey, panelIndex + 1);
            final State state = mConfigEditor.apply();
            refresh(state, panelKey);
        }
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
