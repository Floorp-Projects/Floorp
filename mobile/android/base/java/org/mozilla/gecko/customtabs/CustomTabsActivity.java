/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.os.Bundle;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;

public class CustomTabsActivity extends GeckoApp {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public int getLayout() {
        return R.layout.customtabs_activity;
    }

    @Override
    public void onBackPressed() {
        finish();
    }
}
