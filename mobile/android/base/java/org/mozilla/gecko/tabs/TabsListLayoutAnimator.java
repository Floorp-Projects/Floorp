/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.widget.DefaultItemAnimatorBase;

import android.support.v4.view.ViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;

class TabsListLayoutAnimator extends DefaultItemAnimatorBase {
    public TabsListLayoutAnimator(int animationDuration) {
        setRemoveDuration(animationDuration);
        setAddDuration(animationDuration);
        // A fade in/out each time the title/thumbnail/etc. gets updated isn't helpful, so disable
        // the change animation.
        setSupportsChangeAnimations(false);
    }

    @Override
    protected boolean preAnimateRemoveImpl(final RecyclerView.ViewHolder holder) {
        // If the view isn't at full alpha then we were closed by a swipe which an
        // ItemTouchHelper is animating for us, so just return without animating the remove and
        // let runPendingAnimations pick up the rest.
        if (holder.itemView.getAlpha() < 1) {
            return false;
        }
        resetAnimation(holder);
        return true;
    }

    @Override
    protected void animateRemoveImpl(final RecyclerView.ViewHolder holder) {
        final View itemView = holder.itemView;
        ViewCompat.animate(itemView)
                .setDuration(getRemoveDuration())
                .translationX(itemView.getWidth())
                .alpha(0)
                .setListener(new DefaultRemoveVpaListener(holder))
                .start();
    }

    @Override
    protected boolean preAnimateAddImpl(RecyclerView.ViewHolder holder) {
        resetAnimation(holder);
        final View itemView = holder.itemView;
        itemView.setTranslationX(itemView.getWidth());
        itemView.setAlpha(0);
        return true;
    }

    @Override
    protected void animateAddImpl(final RecyclerView.ViewHolder holder) {
        final View itemView = holder.itemView;
        ViewCompat.animate(itemView)
                .setDuration(getAddDuration())
                .translationX(0)
                .alpha(1)
                .setListener(new DefaultAddVpaListener(holder))
                .start();
    }
}
