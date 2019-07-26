/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test;

import org.mozilla.geckoview.GeckoView;

import android.app.Activity;
import android.content.ContextWrapper;
import android.os.Bundle;

public class GeckoViewTestActivity extends Activity {
    public GeckoView view;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        view = new GeckoView(new ContextWrapper(this));
        setContentView(view);
    }
}
