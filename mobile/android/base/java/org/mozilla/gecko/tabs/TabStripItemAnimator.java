/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.widget.DefaultItemAnimatorBase;

import android.support.v4.view.ViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;

class TabStripItemAnimator extends DefaultItemAnimatorBase {
    public TabStripItemAnimator(int animationDuration) {
        setAddDuration(animationDuration);
        setSupportsChangeAnimations(false);
    }

    @Override
    protected boolean preAnimateRemoveImpl(final RecyclerView.ViewHolder holder) {
        return false;
    }

    @Override
    protected boolean preAnimateAddImpl(final RecyclerView.ViewHolder holder) {
        resetAnimation(holder);
        final View view = holder.itemView;
        view.setTranslationY(view.getHeight());
        return true;
    }

    @Override
    protected void animateAddImpl(final RecyclerView.ViewHolder holder) {
        ViewCompat.animate(holder.itemView)
                .setDuration(getAddDuration())
                .setListener(new DefaultAddVpaListener(holder))
                .translationY(0)
                .start();
    }

    @Override
    protected void resetViewProperties(View view) {
        view.setTranslationX(0);
        view.setTranslationY(0);
    }
}
