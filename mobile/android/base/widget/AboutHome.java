/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import java.util.EnumSet;

import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.R;
import org.mozilla.gecko.ScrollAnimator;
import org.mozilla.gecko.db.BrowserContract;

import android.app.Activity;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

public class AboutHome extends Fragment {
    private UriLoadListener mUriLoadListener;
    private LoadCompleteListener mLoadCompleteListener;
    private LightweightTheme mLightweightTheme;
    private int mTopPadding;
    private AboutHomeView mAboutHomeView;
    private ScrollAnimator mScrollAnimator;

    public interface UriLoadListener {
        public void onAboutHomeUriLoad(String uriSpec);
    }

    public interface LoadCompleteListener {
        public void onAboutHomeLoadComplete();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mLightweightTheme = ((GeckoApplication) getActivity().getApplication()).getLightweightTheme();
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUriLoadListener = (UriLoadListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement UriLoadListener");
        }

        try {
            mLoadCompleteListener = (LoadCompleteListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement LoadCompleteListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUriLoadListener = null;
        mLoadCompleteListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        super.onCreateView(inflater, container, savedInstanceState);

        mAboutHomeView = (AboutHomeView) inflater.inflate(R.layout.abouthome_content, container, false);

        mAboutHomeView.setLightweightTheme(mLightweightTheme);
        mLightweightTheme.addListener(mAboutHomeView);

        // ScrollAnimator implements the View.OnGenericMotionListener
        // interface, which was added in API level 12.
        if (Build.VERSION.SDK_INT >= 12) {
            mScrollAnimator = new ScrollAnimator();
            mAboutHomeView.setOnGenericMotionListener(mScrollAnimator);
        }

        return mAboutHomeView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        view.setPadding(0, mTopPadding, 0, 0);
        ((PromoBox) view.findViewById(R.id.promo_box)).showRandomPromo();
    }

    @Override
    public void onDestroyView() {
        mLightweightTheme.removeListener(mAboutHomeView);

        if (mScrollAnimator != null) {
            mScrollAnimator.cancel();
        }
        mScrollAnimator = null;

        mAboutHomeView = null;

        super.onDestroyView();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Reattach the fragment, forcing a reinflation of its view.
        // We use commitAllowingStateLoss() instead of commit() here to avoid
        // an IllegalStateException. If the phone is rotated while Fennec
        // is in the background, onConfigurationChanged() is fired.
        // onConfigurationChanged() is called before onResume(), so
        // using commit() would throw an IllegalStateException since it can't
        // be used between the Activity's onSaveInstanceState() and
        // onResume().
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    public void requestFocus() {
        View view = getView();
        if (view != null) {
            view.requestFocus();
        }
    }

    public void setTopPadding(int topPadding) {
        View view = getView();
        if (view != null) {
            view.setPadding(0, topPadding, 0, 0);
        }

        // If the padding has changed but the view hasn't been created yet,
        // store the padding values here; they will be used later in
        // onViewCreated().
        mTopPadding = topPadding;
    }

    public int getTopPadding() {
        return mTopPadding;
    }
}
