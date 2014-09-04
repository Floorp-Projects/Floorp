/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * A <code>HomeFragment</code> that, depending on the state of accounts on the
 * device:
 * <ul>
 * <li>displays remote tabs from other devices;</li>
 * <li>offers to re-connect a Firefox Account;</li>
 * <li>offers to create a new Firefox Account.</li>
 * </ul>
 */
public class RemoteTabsPanel extends HomeFragment {
    @SuppressWarnings("unused")
    private static final String LOGTAG = "GeckoRemoteTabsPanel";

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_remote_tabs_panel, container, false);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        loadIfVisible();
    }

    @Override
    protected void load() {
    }
}
