/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.util.ThreadUtils;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.support.v4.animation.AnimatorCompatHelper;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewPropertyAnimator;

import java.util.ArrayList;
import java.util.List;

// This is a light rewrite of DefaultItemAnimator to support a non-default remove animation.
class TabsListLayoutAnimator extends DefaultItemAnimator {
    private List<RecyclerView.ViewHolder> pendingRemovals = new ArrayList<>();
    // ViewHolders on which remove animations are currently being run.
    private List<RecyclerView.ViewHolder> removeAnimations = new ArrayList<>();

    @Override
    public void runPendingAnimations() {
        if (pendingRemovals.isEmpty()) {
            super.runPendingAnimations();
            return;
        }

        for (RecyclerView.ViewHolder holder : pendingRemovals) {
            animateRemoveImpl(holder);
        }
        pendingRemovals.clear();

        // Run remaining default animations, but only after the remove animations finish.
        ThreadUtils.postDelayedToUiThread(new Runnable() {
            @Override
            public void run() {
                TabsListLayoutAnimator.super.runPendingAnimations();
            }
        }, getRemoveDuration());
    }

    @Override
    public boolean animateRemove(RecyclerView.ViewHolder holder) {
        // If the view isn't at full alpha then we were closed by a swipe which an
        // ItemTouchHelper is animating for us, so just return without animating the remove and
        // let runPendingAnimations pick up the rest.
        if (holder.itemView.getAlpha() < 1) {
            dispatchRemoveFinished(holder);
            return false;
        }
        resetAnimation(holder);
        pendingRemovals.add(holder);
        return true;
    }

    private void animateRemoveImpl(final RecyclerView.ViewHolder holder) {
        final TabsLayoutItemView itemView = (TabsLayoutItemView) holder.itemView;
        removeAnimations.add(holder);
        final ViewPropertyAnimator animator = itemView.animate();
        animator.translationX(itemView.getWidth())
                .alpha(0)
                .setDuration(getRemoveDuration())
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        dispatchRemoveStarting(holder);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        animator.setListener(null);
                        itemView.setAlpha(1);
                        itemView.setTranslationX(0);
                        dispatchRemoveFinished(holder);
                        removeAnimations.remove(holder);
                        dispatchFinishedWhenDone();
                    }
                }).start();
    }

    @Override
    public void endAnimation(RecyclerView.ViewHolder item) {
        final View view = item.itemView;
        // This will trigger end callback which should set properties to their target values.
        view.animate().cancel();
        if (pendingRemovals.remove(item)) {
            view.setAlpha(1);
            view.setTranslationX(0);
            dispatchRemoveFinished(item);
        }

        if (isRunning()) {
            super.endAnimation(item);
        } else {
            dispatchAnimationsFinished();
        }
    }

    private void resetAnimation(RecyclerView.ViewHolder holder) {
        AnimatorCompatHelper.clearInterpolator(holder.itemView);
        endAnimation(holder);
    }

    @Override
    public boolean isRunning() {
        return (!pendingRemovals.isEmpty() ||
                !removeAnimations.isEmpty() ||
                super.isRunning());
    }

    /**
     * Check the state of currently pending and running animations. If there are none
     * pending/running, call {@link #dispatchAnimationsFinished()} to notify any
     * listeners.
     */
    private void dispatchFinishedWhenDone() {
        if (!isRunning()) {
            dispatchAnimationsFinished();
        }
    }

    @Override
    public void endAnimations() {
        final int count = pendingRemovals.size();
        for (int i = count - 1; i >= 0; i--) {
            final RecyclerView.ViewHolder item = pendingRemovals.get(i);
            dispatchRemoveFinished(item);
            pendingRemovals.remove(i);
        }

        if (!isRunning()) {
            dispatchAnimationsFinished();
            return;
        }

        cancelAll(removeAnimations);

        // We're relying on super.endAnimations() to call dispatchAnimationsFinished() here.
        super.endAnimations();
    }

    void cancelAll(List<RecyclerView.ViewHolder> viewHolders) {
        for (int i = viewHolders.size() - 1; i >= 0; i--) {
            viewHolders.get(i).itemView.animate().cancel();
        }
    }
}
