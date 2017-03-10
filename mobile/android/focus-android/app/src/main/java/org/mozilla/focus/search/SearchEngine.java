/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.support.annotation.DrawableRes;

/**
 * TODO: This class is only a placeholder until we read our search config files (#184).
 */
public class SearchEngine {
    private String label;
    private int icon;

    /* package */ SearchEngine(@DrawableRes int icon, String label) {
        this.label = label;
        this.icon = icon;
    }

    public String getLabel() {
        return label;
    }

    public int getIcon() {
        return icon;
    }
}
