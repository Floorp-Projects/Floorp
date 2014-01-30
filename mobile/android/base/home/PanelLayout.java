/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;

import android.content.Context;
import android.database.Cursor;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;

import java.util.ArrayList;
import java.util.List;

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

    private final List<ViewEntry> mViewEntries;
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
     * Defines the contract with the component that is responsible
     * for handling datasets requests.
     */
    public interface DatasetHandler {
        /**
         * Requests a dataset to be fetched and auto-bound to the
         * panel views backed by it.
         */
        public void requestDataset(String datasetId);

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
        mViewEntries = new ArrayList<ViewEntry>();
        mDatasetHandler = datasetHandler;
        mUrlOpenListener = urlOpenListener;
    }

    /**
     * Delivers the dataset as a {@code Cursor} to be bound to the
     * panel views backed by it. This is used by the {@code DatasetHandler}
     * in response to a dataset request.
     */
    public final void deliverDataset(String datasetId, Cursor cursor) {
        Log.d(LOGTAG, "Delivering dataset: " + datasetId);
        updateViewsWithDataset(datasetId, cursor);
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
    protected final void requestDataset(String datasetId) {
        Log.d(LOGTAG, "Requesting dataset: " + datasetId);
        mDatasetHandler.requestDataset(datasetId);
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

        final ViewEntry entry = new ViewEntry(view, viewConfig);
        mViewEntries.add(entry);

        ((PanelView) view).setOnUrlOpenListener(mUrlOpenListener);

        return view;
    }

    /**
     * Dispose any dataset references associated with the
     * given view.
     */
    protected final void disposePanelView(View view) {
        Log.d(LOGTAG, "Disposing panel view");

        final int count = mViewEntries.size();
        for (int i = 0; i < count; i++) {
            final View entryView = mViewEntries.get(i).getView();
            if (view == entryView) {
                // Release any Cursor references from the view
                // if it's backed by a dataset.
                maybeSetDataset(entryView, null);

                // Remove the view entry from the list
                mViewEntries.remove(i);
                break;
            }
        }
    }

    private void updateViewsWithDataset(String datasetId, Cursor cursor) {
        final int count = mViewEntries.size();
        for (int i = 0; i < count; i++) {
            final ViewEntry entry = mViewEntries.get(i);

            // Update any views associated with the given dataset ID
            if (TextUtils.equals(entry.getDatasetId(), datasetId)) {
                final View view = entry.getView();
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
     * the {@code PanelLayout}.
     */
    private static class ViewEntry {
        private final View mView;
        private final ViewConfig mViewConfig;

        public ViewEntry(View view, ViewConfig viewConfig) {
            mView = view;
            mViewConfig = viewConfig;
        }

        public View getView() {
            return mView;
        }

        public String getDatasetId() {
            return mViewConfig.getDatasetId();
        }
    }
}
