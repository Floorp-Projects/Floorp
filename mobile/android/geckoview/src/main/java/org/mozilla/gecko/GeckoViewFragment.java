/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.support.v4.app.Fragment;
import android.os.Bundle;
import android.os.Parcelable;
import android.util.Log;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

public class GeckoViewFragment extends android.support.v4.app.Fragment {
    private static final String LOGTAG = "GeckoViewFragment";

    private static Parcelable mSavedState = null;
    private static GeckoViewFragment mLastUsed = null;
    private GeckoView mGeckoView = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        setRetainInstance(true);
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mGeckoView = new GeckoView(getContext());
        return mGeckoView;
    }

    @Override
    public void onResume() {
        if (mSavedState != null && mLastUsed != this) {
            // "Restore" the window from the previously used GeckoView to this GeckoView and attach it
            mGeckoView.onRestoreInstanceState(mSavedState);
            mSavedState = null;
        }
        super.onResume();
    }

    @Override
    public void onPause() {
        mSavedState = mGeckoView.onSaveInstanceState();
        mLastUsed = this;
        super.onPause();
    }
}
