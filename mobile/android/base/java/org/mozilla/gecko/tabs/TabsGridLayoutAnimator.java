/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.widget.DefaultItemAnimatorBase;

import android.support.v7.widget.RecyclerView;

class TabsGridLayoutAnimator extends DefaultItemAnimatorBase {
    public TabsGridLayoutAnimator() {
        setSupportsChangeAnimations(false);
    }

    @Override
    protected boolean preAnimateRemoveImpl(final RecyclerView.ViewHolder holder) {
        return false;
    }
}
