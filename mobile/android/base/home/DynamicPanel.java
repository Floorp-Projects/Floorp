/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.PanelLayout.ContextMenuRegistry;
import org.mozilla.gecko.home.PanelLayout.DatasetHandler;
import org.mozilla.gecko.home.PanelLayout.DatasetRequest;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

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
public class DynamicPanel extends HomeFragment {
    private static final String LOGTAG = "GeckoDynamicPanel";

    // Dataset ID to be used by the loader
    private static final String DATASET_REQUEST = "dataset_request";

    // Max number of items to display in the panel
    private static final int RESULT_LIMIT = 100;

    // The main view for this fragment. This contains the PanelLayout and PanelAuthLayout.
    private FrameLayout mView;

    // The panel layout associated with this panel
    private PanelLayout mPanelLayout;

    // The layout used to show authentication UI for this panel
    private PanelAuthLayout mPanelAuthLayout;

    // Cache used to keep track of whether or not the user has been authenticated.
    private PanelAuthCache mPanelAuthCache;

    // Hold a reference to the UiAsyncTask we use to check the state of the
    // PanelAuthCache, so that we can cancel it if necessary.
    private UiAsyncTask<Void, Void, Boolean> mAuthStateTask;

    // The configuration associated with this panel
    private PanelConfig mPanelConfig;

    // Callbacks used for the loader
    private PanelLoaderCallbacks mLoaderCallbacks;

    // The current UI mode in the fragment
    private UIMode mUIMode;

    /*
     * Different UI modes to display depending on the authentication state.
     *
     * PANEL: Layout to display panel data.
     * AUTH: Authentication UI.
     */
    private enum UIMode {
        PANEL,
        AUTH
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

        mPanelAuthCache = new PanelAuthCache(getActivity());
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mView = new FrameLayout(getActivity());
        return mView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Restore whatever the UI mode the fragment had before
        // a device rotation.
        if (mUIMode != null) {
            setUIMode(mUIMode);
        }

        mPanelAuthCache.setOnChangeListener(new PanelAuthChangeListener());
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mView = null;
        mPanelLayout = null;
        mPanelAuthLayout = null;

        mPanelAuthCache.setOnChangeListener(null);

        if (mAuthStateTask != null) {
            mAuthStateTask.cancel(true);
            mAuthStateTask = null;
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

        if (requiresAuth()) {
            mAuthStateTask = new UiAsyncTask<Void, Void, Boolean>(ThreadUtils.getBackgroundHandler()) {
                @Override
                public synchronized Boolean doInBackground(Void... params) {
                    return mPanelAuthCache.isAuthenticated(mPanelConfig.getId());
                }

                @Override
                public void onPostExecute(Boolean isAuthenticated) {
                    mAuthStateTask = null;
                    setUIMode(isAuthenticated ? UIMode.PANEL : UIMode.AUTH);
                }
            };
            mAuthStateTask.execute();
        } else {
            setUIMode(UIMode.PANEL);
        }
    }

    /**
     * @return true if this panel requires authentication.
     */
    private boolean requiresAuth() {
        return mPanelConfig.getAuthConfig() != null;
    }

    /**
     * Lazily creates layout for panel data.
     */
    private void createPanelLayout() {
        final ContextMenuRegistry contextMenuRegistry = new ContextMenuRegistry() {
            @Override
            public void register(View view) {
                registerForContextMenu(view);
            }
        };

        switch(mPanelConfig.getLayoutType()) {
            case FRAME:
                final PanelDatasetHandler datasetHandler = new PanelDatasetHandler();
                mPanelLayout = new FramePanelLayout(getActivity(), mPanelConfig, datasetHandler,
                        mUrlOpenListener, contextMenuRegistry);
                break;

            default:
                throw new IllegalStateException("Unrecognized layout type in DynamicPanel");
        }

        Log.d(LOGTAG, "Created layout of type: " + mPanelConfig.getLayoutType());
        mView.addView(mPanelLayout);
    }

    /**
     * Lazily creates layout for authentication UI.
     */
    private void createPanelAuthLayout() {
        mPanelAuthLayout = new PanelAuthLayout(getActivity(), mPanelConfig);
        mView.addView(mPanelAuthLayout, 0);
    }

    private void setUIMode(UIMode mode) {
        switch(mode) {
            case PANEL:
                if (mPanelAuthLayout != null) {
                    mPanelAuthLayout.setVisibility(View.GONE);
                }
                if (mPanelLayout == null) {
                    createPanelLayout();
                }
                mPanelLayout.setVisibility(View.VISIBLE);

                // Only trigger a reload if the UI mode has changed
                // (e.g. auth cache changes) and the fragment is allowed
                // to load its contents. Any loaders associated with the
                // panel layout will be automatically re-bound after a
                // device rotation, no need to explicitly load it again.
                if (mUIMode != mode && canLoad()) {
                    mPanelLayout.load();
                }
                break;

            case AUTH:
                if (mPanelLayout != null) {
                    mPanelLayout.setVisibility(View.GONE);
                }
                if (mPanelAuthLayout == null) {
                    createPanelAuthLayout();
                }
                mPanelAuthLayout.setVisibility(View.VISIBLE);
                break;

            default:
                throw new IllegalStateException("Unrecognized UIMode in DynamicPanel");
        }

        mUIMode = mode;
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

            final Bundle bundle = new Bundle();
            bundle.putParcelable(DATASET_REQUEST, request);

            getLoaderManager().restartLoader(request.getViewIndex(),
                                             bundle, mLoaderCallbacks);
        }

        @Override
        public void resetDataset(int viewIndex) {
            Log.d(LOGTAG, "Resetting dataset: " + viewIndex);

            final LoaderManager lm = getLoaderManager();

            // Release any resources associated with the dataset if
            // it's currently loaded in memory.
            final Loader<?> datasetLoader = lm.getLoader(viewIndex);
            if (datasetLoader != null) {
                datasetLoader.reset();
            }
        }
    }

    /**
     * Cursor loader for the panel datasets.
     */
    private static class PanelDatasetLoader extends SimpleCursorLoader {
        private DatasetRequest mRequest;

        public PanelDatasetLoader(Context context, DatasetRequest request) {
            super(context);
            mRequest = request;
        }

        public DatasetRequest getRequest() {
            return mRequest;
        }

        @Override
        public void onContentChanged() {
            // Ensure the refresh request doesn't affect the view's filter
            // stack (i.e. use DATASET_LOAD type) but keep the current
            // dataset ID and filter.
            final DatasetRequest newRequest =
                   new DatasetRequest(mRequest.getViewIndex(),
                                      DatasetRequest.Type.DATASET_LOAD,
                                      mRequest.getDatasetId(),
                                      mRequest.getFilterDetail());

            mRequest = newRequest;
            super.onContentChanged();
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();

            final String selection;
            final String[] selectionArgs;

            // Null represents the root filter
            if (mRequest.getFilter() == null) {
                selection = HomeItems.FILTER + " IS NULL";
                selectionArgs = null;
            } else {
                selection = HomeItems.FILTER + " = ?";
                selectionArgs = new String[] { mRequest.getFilter() };
            }

            final Uri queryUri = HomeItems.CONTENT_URI.buildUpon()
                                                      .appendQueryParameter(BrowserContract.PARAM_DATASET_ID,
                                                                            mRequest.getDatasetId())
                                                      .appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                                            String.valueOf(RESULT_LIMIT))
                                                      .build();

            // XXX: You can use HomeItems.CONTENT_FAKE_URI for development
            // to pull items from fake_home_items.json.
            return cr.query(queryUri, null, selection, selectionArgs, null);
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

            if (mPanelLayout != null) {
                mPanelLayout.deliverDataset(request, cursor);
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final DatasetRequest request = getRequestFromLoader(loader);
            Log.d(LOGTAG, "Resetting loader for request: " + request);

            if (mPanelLayout != null) {
                mPanelLayout.releaseDataset(request.getViewIndex());
            }
        }

        private DatasetRequest getRequestFromLoader(Loader<Cursor> loader) {
            final PanelDatasetLoader datasetLoader = (PanelDatasetLoader) loader;
            return datasetLoader.getRequest();
        }
    }

    private class PanelAuthChangeListener implements PanelAuthCache.OnChangeListener {
        @Override
        public void onChange(String panelId, boolean isAuthenticated) {
            if (!mPanelConfig.getId().equals(panelId)) {
                return;
            }

            setUIMode(isAuthenticated ? UIMode.PANEL : UIMode.AUTH);
        }
    }
}
