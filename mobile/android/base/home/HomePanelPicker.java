/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.PanelInfoManager.RequestCallback;
import org.mozilla.gecko.home.PanelInfoManager.PanelInfo;

/**
 * Dialog for selecting new home panels to add.
 */
public class HomePanelPicker extends FragmentActivity {
    private static final String LOGTAG = "HomePanelPicker";

    /**
     * Intent extra key for caller to pass in a list of existing panel ids.
     */
    public static final String CURRENT_PANELS_IDS = "currentPanelsIds";

    /**
     * Request code when starting the activity with startActivityForResult.
     */
    public static final int REQUEST_CODE_ADD_PANEL = 1;

    private static final int LOADER_ID_CONFIG = 0;

    private ListView mListView;
    private List<String> mCurrentPanelsIds;
    private List<PanelInfo> mPanelInfos;

    @Override
    public void onCreate(Bundle savedInstance) {
        super.onCreate(savedInstance);
        setContentView(R.layout.home_panel_picker);

        // Fetch current panel ids from extras bundle, if provided by caller.
        Bundle intentExtras = getIntent().getExtras();
        if (intentExtras != null) {
            String[] panelIdsArray = intentExtras.getStringArray(CURRENT_PANELS_IDS);
            if (panelIdsArray != null) {
                mCurrentPanelsIds = Arrays.asList(panelIdsArray);
            }
        }

        mListView = (ListView) findViewById(R.id.list);

        // Set an empty adapter.
        final PickerAdapter adapter = new PickerAdapter(this);
        mListView.setAdapter(adapter);

        requestAvailablePanels();
    }

    /**
     * Get panels and update the adapter to display the ones
     * available for install.
     */
    private void requestAvailablePanels() {
        final PanelInfoManager pm = new PanelInfoManager();
        pm.requestAvailablePanels(new RequestCallback() {
            @Override
            public void onComplete(final List<PanelInfo> panelInfos) {
                mPanelInfos = panelInfos;

                // Fetch current home panels if necessary.
                if (mCurrentPanelsIds == null) {
                    loadConfig();
                } else {
                    updatePanelsAdapter(mPanelInfos);
                }
            }
        });
    }

    /**
     * Fetch panels from HomeConfig.
     *
     * This is an async call using Loaders.
     */
    private void loadConfig() {
        final ConfigLoaderCallbacks loaderCallbacks = new ConfigLoaderCallbacks();
        final LoaderManager lm = HomePanelPicker.this.getSupportLoaderManager();
        lm.initLoader(LOADER_ID_CONFIG, null, loaderCallbacks);
    }

    /**
     * Update the list adapter to contain only available panels (with the
     * installed panels filtered out).
     *
     * @param panelInfos List of all available panels.
     */
    private void updatePanelsAdapter(List<PanelInfo> panelInfos) {
        final List<PanelInfo> availablePanels = new ArrayList<PanelInfo>();

        // Filter out panels that are already displayed.
        for (PanelInfo panelInfo : panelInfos) {
            if (!mCurrentPanelsIds.contains(panelInfo.getId())) {
                availablePanels.add(panelInfo);
            }
        }

        if (availablePanels.isEmpty()) {
            setContentView(R.layout.home_panel_picker_empty);
            return;
        }

        final PickerAdapter adapter = (PickerAdapter) mListView.getAdapter();
        adapter.updateFromPanelInfos(availablePanels);
    }

    private void installNewPanelAndQuit(PanelInfo panelInfo) {
        final PanelConfig newPanelConfig = panelInfo.toPanelConfig();
        HomePanelsManager.getInstance().installPanel(newPanelConfig);
        showToastForNewPanel(newPanelConfig);

        setResult(Activity.RESULT_OK);
        finish();
    }

    private void showToastForNewPanel(PanelConfig panelConfig) {
        String panelName = panelConfig.getTitle();

        // Display toast.
        final String successMsg = getResources().getString(R.string.home_add_panel_installed, panelName);
        Toast.makeText(this, successMsg, Toast.LENGTH_SHORT).show();
    }

    // ViewHolder for rows of the panel picker.
    private static class PanelRow {
        final TextView title;
        int position;

        public PanelRow(View view) {
            title = (TextView) view.findViewById(R.id.title);
        }
    }

    private class PickerAdapter extends BaseAdapter {
        private final LayoutInflater mInflater;
        private List<PanelInfo> mPanelInfos;
        private final OnClickListener mOnClickListener;

        public PickerAdapter(Context context) {
            this(context, null);
        }

        public PickerAdapter(Context context, List<PanelInfo> panelInfos) {
            mInflater = LayoutInflater.from(context);
            mPanelInfos = panelInfos;

            mOnClickListener = new OnClickListener() {
                @Override
                public void onClick(View view) {
                    final PanelRow panelsRow = (PanelRow) view.getTag();
                    installNewPanelAndQuit(mPanelInfos.get(panelsRow.position));
                }
            };
        }

        @Override
        public int getCount() {
            if (mPanelInfos == null) {
                return 0;
            }

            return mPanelInfos.size();
        }

        @Override
        public PanelInfo getItem(int position) {
            if (mPanelInfos == null) {
                return null;
            }

            return mPanelInfos.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            PanelRow row;

            if (convertView == null) {
                convertView = mInflater.inflate(R.layout.home_panel_picker_row, null);
                convertView.setOnClickListener(mOnClickListener);

                row = new PanelRow(convertView);
                convertView.setTag(row);
            } else {
                row = (PanelRow) convertView.getTag();
            }

            row.title.setText(mPanelInfos.get(position).getTitle());
            row.position = position;

            return convertView;
        }

        public void updateFromPanelInfos(List<PanelInfo> panelInfos) {
            mPanelInfos = panelInfos;
            notifyDataSetChanged();
        }
    }

    /**
     * Fetch installed Home panels and update the adapter for this activity.
     */
    private class ConfigLoaderCallbacks implements LoaderCallbacks<HomeConfig.State> {
        @Override
        public Loader<HomeConfig.State> onCreateLoader(int id, Bundle args) {
            final HomeConfig homeConfig = HomeConfig.getDefault(HomePanelPicker.this);
            return new HomeConfigLoader(HomePanelPicker.this, homeConfig);
        }

        @Override
        public void onLoadFinished(Loader<HomeConfig.State> loader, HomeConfig.State configState) {
            mCurrentPanelsIds = new ArrayList<String>();
            for (PanelConfig panelConfig : configState) {
                mCurrentPanelsIds.add(panelConfig.getId());
            }

            updatePanelsAdapter(mPanelInfos);
        }

        @Override
        public void onLoaderReset(Loader<HomeConfig.State> loader) {}
    }
}
