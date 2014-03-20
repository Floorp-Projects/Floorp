/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.db.DBUtils;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PanelLayout.DatasetHandler;
import org.mozilla.gecko.home.PanelLayout.DatasetRequest;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * Fragment that displays dynamic content specified by a {@code PanelConfig}.
 * The {@code DynamicPanel} UI is built based on the given {@code LayoutType}
 * and its associated list of {@code ViewConfig}.
 *
 * {@code DynamicPanel} manages all necessary Loaders to load panel datasets
 * from their respective content providers. Each panel dataset has its own
 * associated Loader. This is enforced by defining the Loader IDs based on
 * their associated dataset IDs.
 *
 * The {@code PanelLayout} can make load and reset requests on datasets via
 * the provided {@code DatasetHandler}. This way it doesn't need to know the
 * details of how datasets are loaded and reset. Each time a dataset is
 * requested, {@code DynamicPanel} restarts a Loader with the respective ID (see
 * {@code PanelDatasetHandler}).
 *
 * See {@code PanelLayout} for more details on how {@code DynamicPanel}
 * receives dataset requests and delivers them back to the {@code PanelLayout}.
 */
public class DynamicPanel extends HomeFragment
                          implements GeckoEventListener {
    private static final String LOGTAG = "GeckoDynamicPanel";

    // Dataset ID to be used by the loader
    private static final String DATASET_REQUEST = "dataset_request";

    // The panel layout associated with this panel
    private PanelLayout mLayout;

    // The configuration associated with this panel
    private PanelConfig mPanelConfig;

    // Callbacks used for the loader
    private PanelLoaderCallbacks mLoaderCallbacks;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Bundle args = getArguments();
        if (args != null) {
            mPanelConfig = (PanelConfig) args.getParcelable(HomePager.PANEL_CONFIG_ARG);
        }

        if (mPanelConfig == null) {
            throw new IllegalStateException("Can't create a DynamicPanel without a PanelConfig");
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        switch(mPanelConfig.getLayoutType()) {
            case FRAME:
                final PanelDatasetHandler datasetHandler = new PanelDatasetHandler();
                mLayout = new FramePanelLayout(getActivity(), mPanelConfig, datasetHandler, mUrlOpenListener);
                break;

            default:
                throw new IllegalStateException("Unrecognized layout type in DynamicPanel");
        }

        Log.d(LOGTAG, "Created layout of type: " + mPanelConfig.getLayoutType());

        return mLayout;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        GeckoAppShell.registerEventListener("HomePanels:RefreshDataset", this);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mLayout = null;

        GeckoAppShell.unregisterEventListener("HomePanels:RefreshDataset", this);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Detach and reattach the fragment as the layout changes.
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Create callbacks before the initial loader is started.
        mLoaderCallbacks = new PanelLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void load() {
        Log.d(LOGTAG, "Loading layout");
        mLayout.load();
    }

    @Override
    public void handleMessage(String event, final JSONObject message) {
        if (event.equals("HomePanels:RefreshDataset")) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    handleDatasetRefreshRequest(message);
                }
            });
        }
    }

    private static int generateLoaderId(String datasetId) {
        return datasetId.hashCode();
    }

    /**
     * Handles a dataset refresh request from Gecko. This is usually
     * triggered by a HomeStorage.save() call in an add-on.
     */
    private void handleDatasetRefreshRequest(JSONObject message) {
        final String datasetId;
        try {
            datasetId = message.getString("datasetId");
        } catch (JSONException e) {
            Log.e(LOGTAG, "Failed to handle dataset refresh", e);
            return;
        }

        Log.d(LOGTAG, "Refresh request for dataset: " + datasetId);

        final int loaderId = generateLoaderId(datasetId);

        final LoaderManager lm = getLoaderManager();
        final Loader<?> loader = (Loader<?>) lm.getLoader(loaderId);

        // Only restart a loader if there's already an active one
        // for the given dataset ID. Do nothing otherwise.
        if (loader != null) {
            final PanelDatasetLoader datasetLoader = (PanelDatasetLoader) loader;
            final DatasetRequest request = datasetLoader.getRequest();

            // Ensure the refresh request doesn't affect the view's filter
            // stack (i.e. use DATASET_LOAD type) but keep the current
            // dataset ID and filter.
            final DatasetRequest newRequest =
                   new DatasetRequest(DatasetRequest.Type.DATASET_LOAD,
                                      request.getDatasetId(),
                                      request.getFilterDetail());

            restartDatasetLoader(newRequest);
        }
    }

    private void restartDatasetLoader(DatasetRequest request) {
        final Bundle bundle = new Bundle();
        bundle.putParcelable(DATASET_REQUEST, request);

        // Ensure one loader per dataset
        final int loaderId = generateLoaderId(request.getDatasetId());
        getLoaderManager().restartLoader(loaderId, bundle, mLoaderCallbacks);
    }

    /**
     * Used by the PanelLayout to make load and reset requests to
     * the holding fragment.
     */
    private class PanelDatasetHandler implements DatasetHandler {
        @Override
        public void requestDataset(DatasetRequest request) {
            Log.d(LOGTAG, "Requesting request: " + request);

            // Ignore dataset requests while the fragment is not
            // allowed to load its content.
            if (!getCanLoadHint()) {
                return;
            }

            restartDatasetLoader(request);
        }

        @Override
        public void resetDataset(String datasetId) {
            Log.d(LOGTAG, "Resetting dataset: " + datasetId);

            final LoaderManager lm = getLoaderManager();
            final int loaderId = generateLoaderId(datasetId);

            // Release any resources associated with the dataset if
            // it's currently loaded in memory.
            final Loader<?> datasetLoader = lm.getLoader(loaderId);
            if (datasetLoader != null) {
                datasetLoader.reset();
            }
        }
    }

    /**
     * Cursor loader for the panel datasets.
     */
    private static class PanelDatasetLoader extends SimpleCursorLoader {
        private final DatasetRequest mRequest;

        public PanelDatasetLoader(Context context, DatasetRequest request) {
            super(context);
            mRequest = request;
        }

        public DatasetRequest getRequest() {
            return mRequest;
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();

            final String selection;
            final String[] selectionArgs;

            // Null represents the root filter
            if (mRequest.getFilter() == null) {
                selection = DBUtils.concatenateWhere(HomeItems.DATASET_ID + " = ?", HomeItems.FILTER + " IS NULL");
                selectionArgs = new String[] { mRequest.getDatasetId() };
            } else {
                selection = DBUtils.concatenateWhere(HomeItems.DATASET_ID + " = ?", HomeItems.FILTER + " = ?");
                selectionArgs = new String[] { mRequest.getDatasetId(), mRequest.getFilter() };
            }

            // XXX: You can use CONTENT_FAKE_URI for development to pull items from fake_home_items.json.
            return cr.query(HomeItems.CONTENT_URI, null, selection, selectionArgs, null);
        }
    }

    /**
     * LoaderCallbacks implementation that interacts with the LoaderManager.
     */
    private class PanelLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final DatasetRequest request = (DatasetRequest) args.getParcelable(DATASET_REQUEST);

            Log.d(LOGTAG, "Creating loader for request: " + request);
            return new PanelDatasetLoader(getActivity(), request);
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
            final DatasetRequest request = getRequestFromLoader(loader);

            Log.d(LOGTAG, "Finished loader for request: " + request);
            mLayout.deliverDataset(request, cursor);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final DatasetRequest request = getRequestFromLoader(loader);
            Log.d(LOGTAG, "Resetting loader for request: " + request);
            if (mLayout != null) {
                mLayout.releaseDataset(request.getDatasetId());
            }
        }

        private DatasetRequest getRequestFromLoader(Loader<Cursor> loader) {
            final PanelDatasetLoader datasetLoader = (PanelDatasetLoader) loader;
            return datasetLoader.getRequest();
        }
    }
}
