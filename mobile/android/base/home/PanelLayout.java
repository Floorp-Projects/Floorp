/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.HomeConfig.ItemHandler;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.util.StringUtils;

import android.content.Context;
import android.database.Cursor;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.widget.FrameLayout;

import java.lang.ref.SoftReference;
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

    protected final SparseArray<ViewState> mViewStates;
    private final PanelConfig mPanelConfig;
    private final DatasetHandler mDatasetHandler;
    private final OnUrlOpenListener mUrlOpenListener;

    /**
     * To be used by panel views to express that they are
     * backed by datasets.
     */
    public interface DatasetBacked {
        public void setDataset(Cursor cursor);
        public void setFilterManager(FilterManager manager);
    }

    /**
     * To be used by requests made to {@code DatasetHandler}s to couple dataset ID with current
     * filter for queries on the database.
     */
    public static class DatasetRequest implements Parcelable {
        public enum Type implements Parcelable {
            DATASET_LOAD,
            FILTER_PUSH,
            FILTER_POP;

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeInt(ordinal());
            }

            public static final Creator<Type> CREATOR = new Creator<Type>() {
                @Override
                public Type createFromParcel(final Parcel source) {
                    return Type.values()[source.readInt()];
                }

                @Override
                public Type[] newArray(final int size) {
                    return new Type[size];
                }
            };
        }

        private final int mViewIndex;
        private final Type mType;
        private final String mDatasetId;
        private final FilterDetail mFilterDetail;

        private DatasetRequest(Parcel in) {
            this.mViewIndex = in.readInt();
            this.mType = (Type) in.readParcelable(getClass().getClassLoader());
            this.mDatasetId = in.readString();
            this.mFilterDetail = (FilterDetail) in.readParcelable(getClass().getClassLoader());
        }

        public DatasetRequest(int index, String datasetId, FilterDetail filterDetail) {
            this(index, Type.DATASET_LOAD, datasetId, filterDetail);
        }

        public DatasetRequest(int index, Type type, String datasetId, FilterDetail filterDetail) {
            this.mViewIndex = index;
            this.mType = type;
            this.mDatasetId = datasetId;
            this.mFilterDetail = filterDetail;
        }

        public int getViewIndex() {
            return mViewIndex;
        }

        public Type getType() {
            return mType;
        }

        public String getDatasetId() {
            return mDatasetId;
        }

        public String getFilter() {
            return (mFilterDetail != null ? mFilterDetail.filter : null);
        }

        public FilterDetail getFilterDetail() {
            return mFilterDetail;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(mViewIndex);
            dest.writeParcelable(mType, 0);
            dest.writeString(mDatasetId);
            dest.writeParcelable(mFilterDetail, 0);
        }

        public String toString() {
            return "{ index: " + mViewIndex +
                   ", type: " + mType +
                   ", dataset: " + mDatasetId +
                   ", filter: " + mFilterDetail +
                   " }";
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
         * Releases any resources associated with a panel view. It will
         * do nothing if the view with the given index been created
         * before.
         */
        public void resetDataset(int viewIndex);
    }

    public interface PanelView {
        public void setOnItemOpenListener(OnItemOpenListener listener);
        public void setOnKeyListener(OnKeyListener listener);
    }

    public interface FilterManager {
        public FilterDetail getPreviousFilter();
        public boolean canGoBack();
        public void goBack();
    }

    public PanelLayout(Context context, PanelConfig panelConfig, DatasetHandler datasetHandler, OnUrlOpenListener urlOpenListener) {
        super(context);
        mViewStates = new SparseArray<ViewState>();
        mPanelConfig = panelConfig;
        mDatasetHandler = datasetHandler;
        mUrlOpenListener = urlOpenListener;
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        final int count = mViewStates.size();
        for (int i = 0; i < count; i++) {
            final ViewState viewState = mViewStates.valueAt(i);

            final View view = viewState.getView();
            if (view != null) {
                maybeSetDataset(view, null);
            }
        }
        mViewStates.clear();
    }

    /**
     * Delivers the dataset as a {@code Cursor} to be bound to the
     * panel view backed by it. This is used by the {@code DatasetHandler}
     * in response to a dataset request.
     */
    public final void deliverDataset(DatasetRequest request, Cursor cursor) {
        Log.d(LOGTAG, "Delivering request: " + request);
        final ViewState viewState = mViewStates.get(request.getViewIndex());
        if (viewState == null) {
            return;
        }

        switch (request.getType()) {
            case FILTER_PUSH:
                viewState.pushFilter(request.getFilterDetail());
                break;
            case FILTER_POP:
                viewState.popFilter();
                break;
        }

        final View view = viewState.getView();
        if (view != null) {
            maybeSetDataset(view, cursor);
        }
    }

    /**
     * Releases any references to the given dataset from all
     * existing panel views.
     */
    public final void releaseDataset(int viewIndex) {
        Log.d(LOGTAG, "Releasing dataset: " + viewIndex);
        final ViewState viewState = mViewStates.get(viewIndex);
        if (viewState == null) {
            return;
        }

        final View view = viewState.getView();
        if (view != null) {
            maybeSetDataset(view, null);
        }
    }

    /**
     * Requests a dataset to be loaded and bound to any existing
     * panel view backed by it.
     */
    protected final void requestDataset(DatasetRequest request) {
        Log.d(LOGTAG, "Requesting request: " + request);
        if (mViewStates.get(request.getViewIndex()) == null) {
            return;
        }

        mDatasetHandler.requestDataset(request);
    }

    /**
     * Releases any resources associated with a panel view.
     * e.g. close any associated {@code Cursor}.
     */
    protected final void resetDataset(int viewIndex) {
        Log.d(LOGTAG, "Resetting view with index: " + viewIndex);
        if (mViewStates.get(viewIndex) == null) {
            return;
        }

        mDatasetHandler.resetDataset(viewIndex);
    }

    /**
     * Factory method to create instance of panels from a given
     * {@code ViewConfig}. All panel views defined in {@code PanelConfig}
     * should be created using this method so that {@PanelLayout} can
     * keep track of panel views and their associated datasets.
     */
    protected final View createPanelView(ViewConfig viewConfig) {
        Log.d(LOGTAG, "Creating panel view: " + viewConfig.getType());

        ViewState viewState = mViewStates.get(viewConfig.getIndex());
        if (viewState == null) {
            viewState = new ViewState(viewConfig);
            mViewStates.put(viewConfig.getIndex(), viewState);
        }

        View view = viewState.getView();
        if (view == null) {
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

            PanelView panelView = (PanelView) view;
            panelView.setOnItemOpenListener(new PanelOnItemOpenListener(viewState));
            panelView.setOnKeyListener(new PanelKeyListener(viewState));

            if (view instanceof DatasetBacked) {
                DatasetBacked datasetBacked = (DatasetBacked) view;
                datasetBacked.setFilterManager(new PanelFilterManager(viewState));
            }

            viewState.setView(view);
        }

        return view;
    }

    /**
     * Dispose any dataset references associated with the
     * given view.
     */
    protected final void disposePanelView(View view) {
        Log.d(LOGTAG, "Disposing panel view");
        final int count = mViewStates.size();
        for (int i = 0; i < count; i++) {
            final ViewState viewState = mViewStates.valueAt(i);

            if (viewState.getView() == view) {
                maybeSetDataset(view, null);
                mViewStates.remove(viewState.getIndex());
                break;
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
    protected class ViewState {
        private final ViewConfig mViewConfig;
        private SoftReference<View> mView;
        private LinkedList<FilterDetail> mFilterStack;

        public ViewState(ViewConfig viewConfig) {
            mViewConfig = viewConfig;
            mView = new SoftReference<View>(null);
        }

        public int getIndex() {
            return mViewConfig.getIndex();
        }

        public View getView() {
            return mView.get();
        }

        public void setView(View view) {
            mView = new SoftReference<View>(view);
        }

        public String getDatasetId() {
            return mViewConfig.getDatasetId();
        }

        public ItemHandler getItemHandler() {
            return mViewConfig.getItemHandler();
        }

        /**
         * Get the current filter that this view is displaying, or null if none.
         */
        public FilterDetail getCurrentFilter() {
            if (mFilterStack == null) {
                return null;
            } else {
                return mFilterStack.peek();
            }
        }

        /**
         * Get the previous filter that this view was displaying, or null if none.
         */
        public FilterDetail getPreviousFilter() {
            if (!canPopFilter()) {
                return null;
            }

            return mFilterStack.get(1);
        }

        /**
         * Adds a filter to the history stack for this view.
         */
        public void pushFilter(FilterDetail filter) {
            if (mFilterStack == null) {
                mFilterStack = new LinkedList<FilterDetail>();

                // Initialize with the initial filter.
                mFilterStack.push(new FilterDetail(mViewConfig.getFilter(),
                                                   mPanelConfig.getTitle()));
            }

            mFilterStack.push(filter);
        }

        /**
         * Remove the most recent filter from the stack.
         *
         * @return whether the filter was popped
         */
        public boolean popFilter() {
            if (!canPopFilter()) {
                return false;
            }

            mFilterStack.pop();
            return true;
        }

        public boolean canPopFilter() {
            return (mFilterStack != null && mFilterStack.size() > 1);
        }
    }

    static class FilterDetail implements Parcelable {
        final String filter;
        final String title;

        private FilterDetail(Parcel in) {
            this.filter = in.readString();
            this.title = in.readString();
        }

        public FilterDetail(String filter, String title) {
            this.filter = filter;
            this.title = title;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(filter);
            dest.writeString(title);
        }

        public static final Creator<FilterDetail> CREATOR = new Creator<FilterDetail>() {
            public FilterDetail createFromParcel(Parcel in) {
                return new FilterDetail(in);
            }

            public FilterDetail[] newArray(int size) {
                return new FilterDetail[size];
            }
        };
    }

    /**
     * Pushes filter to {@code ViewState}'s stack and makes request for new filter value.
     */
    private void pushFilterOnView(ViewState viewState, FilterDetail filterDetail) {
        final int index = viewState.getIndex();
        final String datasetId = viewState.getDatasetId();

        mDatasetHandler.requestDataset(new DatasetRequest(index,
                                                          DatasetRequest.Type.FILTER_PUSH,
                                                          datasetId,
                                                          filterDetail));
    }

    /**
     * Pops filter from {@code ViewState}'s stack and makes request for previous filter value.
     *
     * @return whether the filter has changed
     */
    private boolean popFilterOnView(ViewState viewState) {
        if (viewState.canPopFilter()) {
            final int index = viewState.getIndex();
            final String datasetId = viewState.getDatasetId();
            final FilterDetail filterDetail = viewState.getPreviousFilter();

            mDatasetHandler.requestDataset(new DatasetRequest(index,
                                                              DatasetRequest.Type.FILTER_POP,
                                                              datasetId,
                                                              filterDetail));

            return true;
        } else {
            return false;
        }
    }

    public interface OnItemOpenListener {
        public void onItemOpen(String url, String title);
    }

    private class PanelOnItemOpenListener implements OnItemOpenListener {
        private ViewState mViewState;

        public PanelOnItemOpenListener(ViewState viewState) {
            mViewState = viewState;
        }

        @Override
        public void onItemOpen(String url, String title) {
            if (StringUtils.isFilterUrl(url)) {
                FilterDetail filterDetail = new FilterDetail(StringUtils.getFilterFromUrl(url), title);
                pushFilterOnView(mViewState, filterDetail);
            } else {
                EnumSet<OnUrlOpenListener.Flags> flags = EnumSet.noneOf(OnUrlOpenListener.Flags.class);
                if (mViewState.getItemHandler() == ItemHandler.INTENT) {
                    flags.add(OnUrlOpenListener.Flags.OPEN_WITH_INTENT);
                }

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

    private class PanelFilterManager implements FilterManager {
        private final ViewState mViewState;

        public PanelFilterManager(ViewState viewState) {
            mViewState = viewState;
        }

        @Override
        public FilterDetail getPreviousFilter() {
            return mViewState.getPreviousFilter();
        }

        @Override
        public boolean canGoBack() {
            return mViewState.canPopFilter();
        }

        @Override
        public void goBack() {
            popFilterOnView(mViewState);
        }
    }
}
