/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.support.v4.app.Fragment;

public class FirstrunPanel extends Fragment {

    public static final int TITLE_RES = -1;
    public interface PagerNavigation {
        void next();
        void finish();
    }
    protected PagerNavigation pagerNavigation;

    public void setPagerNavigation(PagerNavigation listener) {
        this.pagerNavigation = listener;
    }

    protected void next() {
        if (pagerNavigation != null) {
            pagerNavigation.next();
        }
    }

    protected void close() {
        if (pagerNavigation != null) {
            pagerNavigation.finish();
        }
    }
}
