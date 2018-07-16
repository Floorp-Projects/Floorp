/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import android.support.annotation.NonNull;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.ViewPropertyAnimatorCompat;
import android.support.v4.view.ViewPropertyAnimatorListener;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.SimpleItemAnimator;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * This basically follows the approach taken by Wasabeef:
 *   <a href="https://github.com/wasabeef/recyclerview-animators">https://github.com/wasabeef/recyclerview-animators</a>
 * based off of Android's DefaultItemAnimator from October 2016:
 *   <a href="https://github.com/android/platform_frameworks_support/blob/432f3317f8a9b8cf98277938ea5df4021e983055/v7/recyclerview/src/android/support/v7/widget/DefaultItemAnimator.java">
 *     https://github.com/android/platform_frameworks_support/blob/432f3317f8a9b8cf98277938ea5df4021e983055/v7/recyclerview/src/android/support/v7/widget/DefaultItemAnimator.java
 *   </a>
 * <p>
 * Usage Notes:
 * </p>
 * <ul>
 *   <li>You <strong>must</strong> add a Default*VpaListener to your animate*Impl animation - the
 *       listener takes care of animation bookkeeping.</li>
 *   <li>You should call {@link #resetAnimation(RecyclerView.ViewHolder)} at some point in
 *       preAnimate*Impl if you choose to proceed with the animation. Some animations will want to
 *       know some or all of the current animation values for initializing their own animation
 *       values before resetting the current animation, so this class does not provide the reset
 *       service itself.</li>
 *   <li>{@link #resetViewProperties(View)} is used to reset a view any time an animation ends or
 *       gets canceled - you should redefine resetViewProperties if the version here doesn't reset
 *       all of the properties you're animating.</li>
 * </ul>
 */
public class DefaultItemAnimatorBase extends SimpleItemAnimator {
    private static TimeInterpolator defaultInterpolator;

    private List<RecyclerView.ViewHolder> pendingRemovals = new ArrayList<>();
    private List<RecyclerView.ViewHolder> pendingAdditions = new ArrayList<>();
    private List<MoveInfo> pendingMoves = new ArrayList<>();
    private List<ChangeInfo> pendingChanges = new ArrayList<>();

    private List<List<RecyclerView.ViewHolder>> additionsList = new ArrayList<>();
    private List<List<MoveInfo>> movesList = new ArrayList<>();
    private List<List<ChangeInfo>> changesList = new ArrayList<>();

    private List<RecyclerView.ViewHolder> addAnimations = new ArrayList<>();
    private List<RecyclerView.ViewHolder> moveAnimations = new ArrayList<>();
    private List<RecyclerView.ViewHolder> removeAnimations = new ArrayList<>();
    private List<RecyclerView.ViewHolder> changeAnimations = new ArrayList<>();

    protected static class MoveInfo {
        public RecyclerView.ViewHolder holder;
        public int fromX, fromY, toX, toY;

        public MoveInfo(RecyclerView.ViewHolder holder, int fromX, int fromY, int toX, int toY) {
            this.holder = holder;
            this.fromX = fromX;
            this.fromY = fromY;
            this.toX = toX;
            this.toY = toY;
        }
    }

    protected static class ChangeInfo {
        public RecyclerView.ViewHolder oldHolder, newHolder;
        public int fromX, fromY, toX, toY;

        public ChangeInfo(RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder) {
            this.oldHolder = oldHolder;
            this.newHolder = newHolder;
        }

        public ChangeInfo(RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder,
                           int fromX, int fromY, int toX, int toY) {
            this(oldHolder, newHolder);
            this.fromX = fromX;
            this.fromY = fromY;
            this.toX = toX;
            this.toY = toY;
        }

        @Override
        public String toString() {
            return "ChangeInfo{" +
                    "oldHolder=" + oldHolder +
                    ", newHolder=" + newHolder +
                    ", fromX=" + fromX +
                    ", fromY=" + fromY +
                    ", toX=" + toX +
                    ", toY=" + toY +
                    '}';
        }
    }

    @Override
    public void runPendingAnimations() {
        final boolean removalsPending = !pendingRemovals.isEmpty();
        final boolean movesPending = !pendingMoves.isEmpty();
        final boolean changesPending = !pendingChanges.isEmpty();
        final boolean additionsPending = !pendingAdditions.isEmpty();
        if (!removalsPending && !movesPending && !additionsPending && !changesPending) {
            return;
        }
        // First, remove stuff.
        for (final RecyclerView.ViewHolder holder : pendingRemovals) {
            animateRemoveImpl(holder);
        }
        pendingRemovals.clear();
        // Next, move stuff.
        if (movesPending) {
            final List<MoveInfo> moves = new ArrayList<>();
            moves.addAll(pendingMoves);
            movesList.add(moves);
            pendingMoves.clear();
            final Runnable mover = new Runnable() {
                @Override
                public void run() {
                    for (final MoveInfo moveInfo : moves) {
                        animateMoveImpl(moveInfo.holder, moveInfo.fromX, moveInfo.fromY,
                                moveInfo.toX, moveInfo.toY);
                    }
                    moves.clear();
                    movesList.remove(moves);
                }
            };
            if (removalsPending) {
                final View view = moves.get(0).holder.itemView;
                ViewCompat.postOnAnimationDelayed(view, mover, getRemoveDuration());
            } else {
                mover.run();
            }
        }
        // Next, change stuff, to run in parallel with move animations.
        if (changesPending) {
            final List<ChangeInfo> changes = new ArrayList<>();
            changes.addAll(pendingChanges);
            changesList.add(changes);
            pendingChanges.clear();
            final Runnable changer = new Runnable() {
                @Override
                public void run() {
                    for (final ChangeInfo change : changes) {
                        animateChangeImpl(change);
                    }
                    changes.clear();
                    changesList.remove(changes);
                }
            };
            if (removalsPending) {
                RecyclerView.ViewHolder holder = changes.get(0).oldHolder;
                ViewCompat.postOnAnimationDelayed(holder.itemView, changer, getRemoveDuration());
            } else {
                changer.run();
            }
        }
        // Next, add stuff.
        if (additionsPending) {
            final List<RecyclerView.ViewHolder> additions = new ArrayList<>();
            additions.addAll(pendingAdditions);
            additionsList.add(additions);
            pendingAdditions.clear();
            final Runnable adder = new Runnable() {
                public void run() {
                    for (final RecyclerView.ViewHolder holder : additions) {
                        animateAddImpl(holder);
                    }
                    additions.clear();
                    additionsList.remove(additions);
                }
            };
            if (removalsPending || movesPending || changesPending) {
                final long removeDuration = removalsPending ? getRemoveDuration() : 0;
                final long moveDuration = movesPending ? getMoveDuration() : 0;
                final long changeDuration = changesPending ? getChangeDuration() : 0;
                final long totalDelay = removeDuration + Math.max(moveDuration, changeDuration);
                final View view = additions.get(0).itemView;
                ViewCompat.postOnAnimationDelayed(view, adder, totalDelay);
            } else {
                adder.run();
            }
        }
    }

    @Override
    public boolean animateRemove(final RecyclerView.ViewHolder holder) {
        if (!preAnimateRemoveImpl(holder)) {
            dispatchRemoveFinished(holder);
            return false;
        }
        pendingRemovals.add(holder);
        return true;
    }

    protected boolean preAnimateRemoveImpl(final RecyclerView.ViewHolder holder) {
        resetAnimation(holder);
        return true;
    }

    protected void animateRemoveImpl(final RecyclerView.ViewHolder holder) {
        ViewCompat.animate(holder.itemView)
                .setDuration(getRemoveDuration())
                .alpha(0)
                .setListener(new DefaultRemoveVpaListener(holder))
                .start();
    }

    @Override
    public boolean animateAdd(final RecyclerView.ViewHolder holder) {
        if (!preAnimateAddImpl(holder)) {
            dispatchAddFinished(holder);
            return false;
        }
        pendingAdditions.add(holder);
        return true;
    }

    protected boolean preAnimateAddImpl(RecyclerView.ViewHolder holder) {
        resetAnimation(holder);
        holder.itemView.setAlpha(0);
        return true;
    }

    protected void animateAddImpl(final RecyclerView.ViewHolder holder) {
        ViewCompat.animate(holder.itemView)
                .setDuration(getAddDuration())
                .alpha(1)
                .setListener(new DefaultAddVpaListener(holder))
                .start();
    }

    @Override
    public boolean animateMove(final RecyclerView.ViewHolder holder,
                               int fromX, int fromY, int toX, int toY) {
        final View view = holder.itemView;
        fromX += ViewCompat.getTranslationX(holder.itemView);
        fromY += ViewCompat.getTranslationY(holder.itemView);
        final int deltaX = toX - fromX;
        final int deltaY = toY - fromY;
        if (deltaX == 0 && deltaY == 0) {
            dispatchMoveFinished(holder);
            return false;
        }
        resetAnimation(holder);
        if (deltaX != 0) {
            ViewCompat.setTranslationX(view, -deltaX);
        }
        if (deltaY != 0) {
            ViewCompat.setTranslationY(view, -deltaY);
        }
        pendingMoves.add(new MoveInfo(holder, fromX, fromY, toX, toY));
        return true;
    }

    protected void animateMoveImpl(final RecyclerView.ViewHolder holder,
                                   int fromX, int fromY, int toX, int toY) {
        final View view = holder.itemView;
        final int deltaX = toX - fromX;
        final int deltaY = toY - fromY;
        if (deltaX != 0) {
            ViewCompat.animate(view).translationX(0);
        }
        if (deltaY != 0) {
            ViewCompat.animate(view).translationY(0);
        }
        // TODO: make EndActions end listeners instead, since end actions aren't called when
        // vpas are canceled (and can't end them. why?)
        // need listener functionality in VPACompat for this. Ick.
        final ViewPropertyAnimatorCompat animation = ViewCompat.animate(view);
        moveAnimations.add(holder);
        animation.setDuration(getMoveDuration()).setListener(new VpaListenerAdapter() {
            @Override
            public void onAnimationStart(View view) {
                dispatchMoveStarting(holder);
            }
            @Override
            public void onAnimationCancel(View view) {
                resetViewProperties(view);
            }
            @Override
            public void onAnimationEnd(View view) {
                animation.setListener(null);
                dispatchMoveFinished(holder);
                moveAnimations.remove(holder);
                dispatchFinishedWhenDone();
            }
        }).start();
    }

    @Override
    public boolean animateChange(RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder,
                                 int fromX, int fromY, int toX, int toY) {
        if (oldHolder == newHolder) {
            // Don't know how to run change animations when the same view holder is re-used.
            // Run a move animation to handle position changes (if there are any).
            if (fromX != toX || fromY != toY) {
                // *Don't* call dispatchChangeFinished here, it leads to unbalanced isRecyclable calls.
                return animateMove(oldHolder, fromX, fromY, toX, toY);
            }
            dispatchChangeFinished(oldHolder, true);
            return false;
        }
        final float prevTranslationX = ViewCompat.getTranslationX(oldHolder.itemView);
        final float prevTranslationY = ViewCompat.getTranslationY(oldHolder.itemView);
        final float prevAlpha = ViewCompat.getAlpha(oldHolder.itemView);
        resetAnimation(oldHolder);
        final int deltaX = (int) (toX - fromX - prevTranslationX);
        final int deltaY = (int) (toY - fromY - prevTranslationY);
        // Recover previous translation state after ending animation.
        ViewCompat.setTranslationX(oldHolder.itemView, prevTranslationX);
        ViewCompat.setTranslationY(oldHolder.itemView, prevTranslationY);
        ViewCompat.setAlpha(oldHolder.itemView, prevAlpha);
        if (newHolder != null) {
            // Carry over translation values.
            resetAnimation(newHolder);
            ViewCompat.setTranslationX(newHolder.itemView, -deltaX);
            ViewCompat.setTranslationY(newHolder.itemView, -deltaY);
            ViewCompat.setAlpha(newHolder.itemView, 0);
        }
        pendingChanges.add(new ChangeInfo(oldHolder, newHolder, fromX, fromY, toX, toY));
        return true;
    }

    protected void animateChangeImpl(final ChangeInfo changeInfo) {
        final RecyclerView.ViewHolder holder = changeInfo.oldHolder;
        final View view = holder == null ? null : holder.itemView;
        final RecyclerView.ViewHolder newHolder = changeInfo.newHolder;
        final View newView = newHolder != null ? newHolder.itemView : null;
        if (view != null) {
            final ViewPropertyAnimatorCompat oldViewAnim = ViewCompat.animate(view).setDuration(
                    getChangeDuration());
            changeAnimations.add(changeInfo.oldHolder);
            oldViewAnim.translationX(changeInfo.toX - changeInfo.fromX);
            oldViewAnim.translationY(changeInfo.toY - changeInfo.fromY);
            oldViewAnim.alpha(0).setListener(new VpaListenerAdapter() {
                @Override
                public void onAnimationStart(View view) {
                    dispatchChangeStarting(changeInfo.oldHolder, true);
                }

                @Override
                public void onAnimationEnd(View view) {
                    oldViewAnim.setListener(null);
                    resetViewProperties(view);
                    dispatchChangeFinished(changeInfo.oldHolder, true);
                    changeAnimations.remove(changeInfo.oldHolder);
                    dispatchFinishedWhenDone();
                }
            }).start();
        }
        if (newView != null) {
            final ViewPropertyAnimatorCompat newViewAnimation = ViewCompat.animate(newView);
            changeAnimations.add(changeInfo.newHolder);
            newViewAnimation.translationX(0).translationY(0).setDuration(getChangeDuration()).
                    alpha(1).setListener(new VpaListenerAdapter() {
                @Override
                public void onAnimationStart(View view) {
                    dispatchChangeStarting(changeInfo.newHolder, false);
                }
                @Override
                public void onAnimationEnd(View view) {
                    newViewAnimation.setListener(null);
                    resetViewProperties(view);
                    dispatchChangeFinished(changeInfo.newHolder, false);
                    changeAnimations.remove(changeInfo.newHolder);
                    dispatchFinishedWhenDone();
                }
            }).start();
}
    }

    private void endChangeAnimation(List<ChangeInfo> infoList, RecyclerView.ViewHolder item) {
        for (int i = infoList.size() - 1; i >= 0; i--) {
            final ChangeInfo changeInfo = infoList.get(i);
            if (endChangeAnimationIfNecessary(changeInfo, item)) {
                if (changeInfo.oldHolder == null && changeInfo.newHolder == null) {
                    infoList.remove(changeInfo);
                }
            }
        }
    }

    private void endChangeAnimationIfNecessary(ChangeInfo changeInfo) {
        if (changeInfo.oldHolder != null) {
            endChangeAnimationIfNecessary(changeInfo, changeInfo.oldHolder);
        }
        if (changeInfo.newHolder != null) {
            endChangeAnimationIfNecessary(changeInfo, changeInfo.newHolder);
        }
    }

    private boolean endChangeAnimationIfNecessary(ChangeInfo changeInfo, RecyclerView.ViewHolder item) {
        boolean oldItem = false;
        if (changeInfo.newHolder == item) {
            changeInfo.newHolder = null;
        } else if (changeInfo.oldHolder == item) {
            changeInfo.oldHolder = null;
            oldItem = true;
        } else {
            return false;
        }
        resetViewProperties(item.itemView);
        dispatchChangeFinished(item, oldItem);
        return true;
    }

    /**
     * Called to reset all properties possibly animated by any and all defined animations.
     */
    protected void resetViewProperties(View view) {
        view.setTranslationX(0);
        view.setTranslationY(0);
        view.setAlpha(1);
    }

    @Override
    public void endAnimation(RecyclerView.ViewHolder item) {

        final View view = item.itemView;
        // This calls dispatch*Finished, resets view properties, and removes item from current
        // animations list if the view is currently being animated.
        ViewCompat.animate(view).cancel();
        // TODO if some other animations are chained to end, how do we cancel them as well?
        for (int i = pendingMoves.size() - 1; i >= 0; i--) {
            final MoveInfo moveInfo = pendingMoves.get(i);
            if (moveInfo.holder == item) {
                resetViewProperties(view);
                dispatchMoveFinished(item);
                pendingMoves.remove(i);
            }
        }
        endChangeAnimation(pendingChanges, item);
        if (pendingRemovals.remove(item)) {
            resetViewProperties(view);
            dispatchRemoveFinished(item);
        }
        if (pendingAdditions.remove(item)) {
            resetViewProperties(view);
            dispatchAddFinished(item);
        }

        for (int i = changesList.size() - 1; i >= 0; i--) {
            final List<ChangeInfo> changes = changesList.get(i);
            endChangeAnimation(changes, item);
            if (changes.isEmpty()) {
                changesList.remove(i);
            }
        }
        for (int i = movesList.size() - 1; i >= 0; i--) {
            final List<MoveInfo> moves = movesList.get(i);
            for (int j = moves.size() - 1; j >= 0; j--) {
                final MoveInfo moveInfo = moves.get(j);
                if (moveInfo.holder == item) {
                    resetViewProperties(view);
                    dispatchMoveFinished(item);
                    moves.remove(j);
                    if (moves.isEmpty()) {
                        movesList.remove(i);
                    }
                    break;
                }
            }
        }
        for (int i = additionsList.size() - 1; i >= 0; i--) {
            final List<RecyclerView.ViewHolder> additions = additionsList.get(i);
            if (additions.remove(item)) {
                resetViewProperties(view);
                dispatchAddFinished(item);
                if (additions.isEmpty()) {
                    additionsList.remove(i);
                }
            }
        }
        dispatchFinishedWhenDone();
    }

    protected void resetAnimation(RecyclerView.ViewHolder holder) {
        if (defaultInterpolator == null) {
            defaultInterpolator = new ValueAnimator().getInterpolator();
        }
        holder.itemView.animate().setInterpolator(defaultInterpolator);
        endAnimation(holder);
    }

    @Override
    public boolean isRunning() {
        return (!pendingAdditions.isEmpty() ||
                !pendingChanges.isEmpty() ||
                !pendingMoves.isEmpty() ||
                !pendingRemovals.isEmpty() ||
                !moveAnimations.isEmpty() ||
                !removeAnimations.isEmpty() ||
                !addAnimations.isEmpty() ||
                !changeAnimations.isEmpty() ||
                !movesList.isEmpty() ||
                !additionsList.isEmpty() ||
                !changesList.isEmpty());
    }

    /**
     * Check the state of currently pending and running animations. If there are none
     * pending/running, call {@link #dispatchAnimationsFinished()} to notify any
     * listeners.
     */
    protected void dispatchFinishedWhenDone() {
        if (!isRunning()) {
            dispatchAnimationsFinished();
        }
    }

    @Override
    public void endAnimations() {
        int count = pendingMoves.size();
        for (int i = count - 1; i >= 0; i--) {
            final MoveInfo item = pendingMoves.get(i);
            resetViewProperties(item.holder.itemView);
            dispatchMoveFinished(item.holder);
            pendingMoves.remove(i);
        }
        count = pendingRemovals.size();
        for (int i = count - 1; i >= 0; i--) {
            final RecyclerView.ViewHolder item = pendingRemovals.get(i);
            resetViewProperties(item.itemView);
            dispatchRemoveFinished(item);
            pendingRemovals.remove(i);
        }
        count = pendingAdditions.size();
        for (int i = count - 1; i >= 0; i--) {
            final RecyclerView.ViewHolder item = pendingAdditions.get(i);
            resetViewProperties(item.itemView);
            dispatchAddFinished(item);
            pendingAdditions.remove(i);
        }
        count = pendingChanges.size();
        for (int i = count - 1; i >= 0; i--) {
            endChangeAnimationIfNecessary(pendingChanges.get(i));
        }
        pendingChanges.clear();
        if (!isRunning()) {
            return;
        }

        int listCount = movesList.size();
        for (int i = listCount - 1; i >= 0; i--) {
            final List<MoveInfo> moves = movesList.get(i);
            count = moves.size();
            for (int j = count - 1; j >= 0; j--) {
                final MoveInfo moveInfo = moves.get(j);
                final RecyclerView.ViewHolder item = moveInfo.holder;
                resetViewProperties(item.itemView);
                dispatchMoveFinished(item);
                moves.remove(j);
                if (moves.isEmpty()) {
                    movesList.remove(moves);
                }
            }
        }
        listCount = additionsList.size();
        for (int i = listCount - 1; i >= 0; i--) {
            final List<RecyclerView.ViewHolder> additions = additionsList.get(i);
            count = additions.size();
            for (int j = count - 1; j >= 0; j--) {
                final RecyclerView.ViewHolder item = additions.get(j);
                resetViewProperties(item.itemView);
                dispatchAddFinished(item);
                additions.remove(j);
                if (additions.isEmpty()) {
                    additionsList.remove(additions);
                }
            }
        }
        listCount = changesList.size();
        for (int i = listCount - 1; i >= 0; i--) {
            final List<ChangeInfo> changes = changesList.get(i);
            count = changes.size();
            for (int j = count - 1; j >= 0; j--) {
                endChangeAnimationIfNecessary(changes.get(j));
                if (changes.isEmpty()) {
                    changesList.remove(changes);
                }
            }
        }

        cancelAll(removeAnimations);
        cancelAll(moveAnimations);
        cancelAll(addAnimations);
        cancelAll(changeAnimations);

        dispatchAnimationsFinished();
    }

    public void cancelAll(List<RecyclerView.ViewHolder> viewHolders) {
        for (int i = viewHolders.size() - 1; i >= 0; i--) {
            ViewCompat.animate(viewHolders.get(i).itemView).cancel();
        }
    }

    /**
     * {@inheritDoc}
     * <p>
     * If the payload list is not empty, DefaultItemAnimator returns <code>true</code>.
     * When this is the case:
     * <ul>
     * <li>If you override
     * {@link #animateChange(RecyclerView.ViewHolder, RecyclerView.ViewHolder, int, int, int, int)},
     * both ViewHolder arguments will be the same instance.
     * </li>
     * <li>
     * If you are not overriding
     * {@link #animateChange(RecyclerView.ViewHolder, RecyclerView.ViewHolder, int, int, int, int)},
     * then DefaultItemAnimator will call
     * {@link #animateMove(RecyclerView.ViewHolder, int, int, int, int)} and run a move animation
     * instead.
     * </li>
     * </ul>
     */
    @Override
    public boolean canReuseUpdatedViewHolder(@NonNull RecyclerView.ViewHolder viewHolder,
            @NonNull List<Object> payloads) {
        return !payloads.isEmpty() || super.canReuseUpdatedViewHolder(viewHolder, payloads);
    }

    private class VpaListenerAdapter implements ViewPropertyAnimatorListener {
        @Override
        public void onAnimationStart(View view) {}

        // Note that onAnimationEnd is called (in addition to OnAnimationCancel) whenever an
        // animation is canceled.
        @Override
        public void onAnimationEnd(View view) {
            resetViewProperties(view);
            view.animate().setListener(null);
        }

        @Override
        public void onAnimationCancel(View view) {}
    }

    protected class DefaultRemoveVpaListener extends VpaListenerAdapter {
        private final RecyclerView.ViewHolder viewHolder;

        public DefaultRemoveVpaListener(final RecyclerView.ViewHolder holder) {
            viewHolder = holder;
        }

        @Override
        public void onAnimationStart(View view) {
            removeAnimations.add(viewHolder);
            dispatchRemoveStarting(viewHolder);
        }

        @Override
        public void onAnimationEnd(View view) {
            removeAnimations.remove(viewHolder);
            dispatchRemoveFinished(viewHolder);
            dispatchFinishedWhenDone();
            super.onAnimationEnd(view);
        }
    }

    protected class DefaultAddVpaListener extends VpaListenerAdapter {
        private final RecyclerView.ViewHolder viewHolder;

        public DefaultAddVpaListener(final RecyclerView.ViewHolder holder) {
            viewHolder = holder;
        }

        @Override
        public void onAnimationStart(View view) {
            addAnimations.add(viewHolder);
            dispatchAddStarting(viewHolder);
        }

        @Override
        public void onAnimationEnd(View view) {
            addAnimations.remove(viewHolder);
            dispatchAddFinished(viewHolder);
            dispatchFinishedWhenDone();
            super.onAnimationEnd(view);
        }
    }
}
