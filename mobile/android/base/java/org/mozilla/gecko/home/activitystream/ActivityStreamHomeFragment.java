/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeFragment;

/**
 * Simple wrapper around the ActivityStream view that allows embedding as a HomePager panel.
 */
public class ActivityStreamHomeFragment
        extends HomeFragment {
    private ActivityStream activityStream;

    @Override
    protected void load() {
        activityStream.load(getLoaderManager());
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        if (activityStream == null) {
            activityStream = (ActivityStream) inflater.inflate(R.layout.activity_stream, container, false);
            activityStream.setOnUrlOpenListener(mUrlOpenListener);
        }

        return activityStream;
    }
}
