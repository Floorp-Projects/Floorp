/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.util.StringUtils;

import android.content.Context;
import android.database.Cursor;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.FrameLayout;

import java.util.Deque;
import java.util.EnumSet;
import java.util.LinkedList;
import java.util.Map;
import java.util.WeakHashMap;

/**
 * {@code PanelLayout} is the base class for custom layouts to be
 * used in {@code DynamicPanel}. It provides the basic framework
 * that enables custom layouts to request and reset datasets and
 * create panel views. Furthermore, it automates most of the process
 * of binding panel views with their respective datasets.
 *
 * {@code PanelLayout} abstracts the implemention details of how
 * datasets are actually loaded through the {@DatasetHandler} interface.
 * {@code DatasetHandler} provides two operations: request and reset.
 * The results of the dataset requests done via the {@code DatasetHandler}
 * are delivered to the {@code PanelLayout} with the {@code deliverDataset()}
 * method.
 *
 * Subclasses of {@code PanelLayout} should simply use the utilities
 * provided by {@code PanelLayout}. Namely:
 *
 * {@code requestDataset()} - To fetch datasets and auto-bind them to
 *                            the existing panel views backed by them.
 *
 * {@code resetDataset()} - To release any resources associated with a
 *                          previously loaded dataset.
 *
 * {@code createPanelView()} - To create a panel view for a ViewConfig
 *                             associated with the panel.
 *
 * {@code disposePanelView()} - To dispose any dataset references associated
 *                              with the given view.
 *
 * {@code PanelLayout} subclasses should always use {@code createPanelView()}
 * to create the views dynamically created based on {@code ViewConfig}. This
 * allows {@code PanelLayout} to auto-bind datasets with panel views.
 * {@code PanelLayout} subclasses are free to have any type of views to arrange
 * the panel views in different ways.
 */
abstract class PanelLayout extends FrameLayout {
    private static final String LOGTAG = "GeckoPanelLayout";

    protected final Map<View, ViewState> mViewStateMap;
    private final DatasetHandler mDatasetHandler;
    private final OnUrlOpenListener mUrlOpenListener;

    /**
     * To be used by panel views to express that they are
     * backed by datasets.
     */
    public interface DatasetBacked {
        public void setDataset(Cursor cursor);
    }

    /**
     * To be used by requests made to {@code DatasetHandler}s to couple dataset ID with current
     * filter for queries on the database.
     */
    public static class DatasetRequest implements Parcelable {
        public final String datasetId;
        public final String filter;

        private DatasetRequest(Parcel in) {
            this.datasetId = in.readString();
            this.filter = in.readString();
        }

        public DatasetRequest(String datasetId, String filter) {
            this.datasetId = datasetId;
            this.filter = filter;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(datasetId);
            dest.writeString(filter);
        }

        public String toString() {
            return "{dataset: " + datasetId + ", filter: " + filter + "}";
        }

        public static final Creator<DatasetRequest> CREATOR = new Creator<DatasetRequest>() {
            public DatasetRequest createFromParcel(Parcel in) {
                return new DatasetRequest(in);
            }

            public DatasetRequest[] newArray(int size) {
                return new DatasetRequest[size];
            }
        };
    }

    /**
     * Defines the contract with the component that is responsible
     * for handling datasets requests.
     */
    public interface DatasetHandler {
        /**
         * Requests a dataset to be fetched and auto-bound to the
         * panel views backed by it.
         */
        public void requestDataset(DatasetRequest request);

        /**
         * Releases any resources associated with a previously loaded
         * dataset. It will do nothing if the dataset with the given ID
         * hasn't been loaded before.
         */
        public void resetDataset(String datasetId);
    }

    public interface PanelView {
        public void setOnUrlOpenListener(OnUrlOpenListener listener);
    }

    public PanelLayout(Context context, PanelConfig panelConfig, DatasetHandler datasetHandler, OnUrlOpenListener urlOpenListener) {
        super(context);
        mViewStateMap = new WeakHashMap<View, ViewState>();
        mDatasetHandler = datasetHandler;
        mUrlOpenListener = urlOpenListener;
    }

    /**
     * Delivers the dataset as a {@code Cursor} to be bound to the
     * panel views backed by it. This is used by the {@code DatasetHandler}
     * in response to a dataset request.
     */
    public final void deliverDataset(DatasetRequest request, Cursor cursor) {
        Log.d(LOGTAG, "Delivering request: " + request);
        updateViewsWithDataset(request.datasetId, cursor);
    }

    /**
     * Releases any references to the given dataset from all
     * existing panel views.
     */
    public final void releaseDataset(String datasetId) {
        Log.d(LOGTAG, "Resetting dataset: " + datasetId);
        updateViewsWithDataset(datasetId, null);
    }

    /**
     * Requests a dataset to be loaded and bound to any existing
     * panel view backed by it.
     */
    protected final void requestDataset(DatasetRequest request) {
        Log.d(LOGTAG, "Requesting request: " + request);
        mDatasetHandler.requestDataset(request);
    }

    /**
     * Releases any resources associated with a previously
     * loaded dataset e.g. close any associated {@code Cursor}.
     */
    protected final void resetDataset(String datasetId) {
        mDatasetHandler.resetDataset(datasetId);
    }

    /**
     * Factory method to create instance of panels from a given
     * {@code ViewConfig}. All panel views defined in {@code PanelConfig}
     * should be created using this method so that {@PanelLayout} can
     * keep track of panel views and their associated datasets.
     */
    protected final View createPanelView(ViewConfig viewConfig) {
        final View view;

        Log.d(LOGTAG, "Creating panel view: " + viewConfig.getType());

        switch(viewConfig.getType()) {
            case LIST:
                view = new PanelListView(getContext(), viewConfig);
                break;

            case GRID:
                view = new PanelGridView(getContext(), viewConfig);
                break;

            default:
                throw new IllegalStateException("Unrecognized view type in " + getClass().getSimpleName());
        }

        final ViewState state = new ViewState(viewConfig);
        // TODO: Push initial filter here onto ViewState
        mViewStateMap.put(view, state);

        ((PanelView) view).setOnUrlOpenListener(new PanelUrlOpenListener(state));
        view.setOnKeyListener(new PanelKeyListener(state));

        return view;
    }

    /**
     * Dispose any dataset references associated with the
     * given view.
     */
    protected final void disposePanelView(View view) {
        Log.d(LOGTAG, "Disposing panel view");
        if (mViewStateMap.containsKey(view)) {
            // Release any Cursor references from the view
            // if it's backed by a dataset.
            maybeSetDataset(view, null);

            // Remove the view entry from the map
            mViewStateMap.remove(view);
        }
    }

    private void updateViewsWithDataset(String datasetId, Cursor cursor) {
        for (Map.Entry<View, ViewState> entry : mViewStateMap.entrySet()) {
            final ViewState detail = entry.getValue();

            // Update any views associated with the given dataset ID
            if (TextUtils.equals(detail.getDatasetId(), datasetId)) {
                final View view = entry.getKey();
                maybeSetDataset(view, cursor);
            }
        }
    }

    private void maybeSetDataset(View view, Cursor cursor) {
        if (view instanceof DatasetBacked) {
            final DatasetBacked dsb = (DatasetBacked) view;
            dsb.setDataset(cursor);
        }
    }

    /**
     * Must be implemented by {@code PanelLayout} subclasses to define
     * what happens then the layout is first loaded. Should set initial
     * UI state and request any necessary datasets.
     */
    public abstract void load();

    /**
     * Represents a 'live' instance of a panel view associated with
     * the {@code PanelLayout}. Is responsible for tracking the history stack of filters.
     */
    protected static class ViewState {
        private final ViewConfig mViewConfig;
        private Deque<String> mFilterStack;

        public ViewState(ViewConfig viewConfig) {
            mViewConfig = viewConfig;
        }

        public String getDatasetId() {
            return mViewConfig.getDatasetId();
        }

        /**
         * Used to find the current filter that this view is displaying, or null if none.
         */
        public String getCurrentFilter() {
            if (mFilterStack == null) {
                return null;
            } else {
                return mFilterStack.peek();
            }
        }

        /**
         * Adds a filter to the history stack for this view.
         */
        public void pushFilter(String filter) {
            if (mFilterStack == null) {
                mFilterStack = new LinkedList<String>();
            }

            mFilterStack.push(filter);
        }

        public String popFilter() {
            if (getCurrentFilter() != null) {
                mFilterStack.pop();
            }

            return getCurrentFilter();
        }
    }

    /**
     * Pushes filter to {@code ViewState}'s stack and makes request for new filter value.
     */
    private void pushFilterOnView(ViewState viewState, String filter) {
        viewState.pushFilter(filter);
        mDatasetHandler.requestDataset(new DatasetRequest(viewState.getDatasetId(), filter));
    }

    /**
     * Pops filter from {@code ViewState}'s stack and makes request for previous filter value.
     *
     * @return whether the filter has changed
     */
    private boolean popFilterOnView(ViewState viewState) {
        String currentFilter = viewState.getCurrentFilter();
        String filter = viewState.popFilter();

        if (!TextUtils.equals(currentFilter, filter)) {
            mDatasetHandler.requestDataset(new DatasetRequest(viewState.getDatasetId(), filter));
            return true;
        } else {
            return false;
        }
    }

    /**
     * Custom listener so that we can intercept any filter URLs and make a new dataset request
     * rather than forwarding them to the default listener.
     */
    private class PanelUrlOpenListener implements OnUrlOpenListener {
        private ViewState mViewState;

        public PanelUrlOpenListener(ViewState viewState) {
            mViewState = viewState;
        }

        @Override
        public void onUrlOpen(String url, EnumSet<Flags> flags) {
            if (StringUtils.isFilterUrl(url)) {
                pushFilterOnView(mViewState, StringUtils.getFilterFromUrl(url));
            } else {
                mUrlOpenListener.onUrlOpen(url, flags);
            }
        }
    }

    private class PanelKeyListener implements View.OnKeyListener {
        private ViewState mViewState;

        public PanelKeyListener(ViewState viewState) {
            mViewState = viewState;
        }

        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
                return popFilterOnView(mViewState);
            }

            return false;
        }
    }
}
