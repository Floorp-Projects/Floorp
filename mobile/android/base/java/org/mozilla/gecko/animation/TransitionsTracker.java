/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.animation;

import com.nineoldandroids.animation.Animator;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.PropertyAnimationListener;
import org.mozilla.gecko.util.ThreadUtils;


/**
 * {@link TransitionsTracker} provides a simple API to avoid running layout code
 * during UI transitions. You should use it whenever you need to time-shift code
 * that will likely trigger a layout traversal during an animation.
 */
public class TransitionsTracker {
    private static final ArrayList<Runnable> pendingActions = new ArrayList<>();
    private static int transitionCount;

    private static final PropertyAnimationListener propertyAnimatorListener =
            new PropertyAnimationListener() {
        @Override
        public void onPropertyAnimationStart() {
            pushTransition();
        }

        @Override
        public void onPropertyAnimationEnd() {
            popTransition();
        }
    };

    private static final Animator.AnimatorListener animatorListener =
            new Animator.AnimatorListener() {
        @Override
        public void onAnimationStart(Animator animation) {
            pushTransition();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            popTransition();
        }

        @Override
        public void onAnimationCancel(Animator animation) {
        }

        @Override
        public void onAnimationRepeat(Animator animation) {
        }
    };

    private static void runPendingActions() {
        ThreadUtils.assertOnUiThread();

        final int size = pendingActions.size();
        for (int i = 0; i < size; i++) {
            pendingActions.get(i).run();
        }

        pendingActions.clear();
    }

    public static void pushTransition() {
        ThreadUtils.assertOnUiThread();
        transitionCount++;
    }

    public static void popTransition() {
        ThreadUtils.assertOnUiThread();
        transitionCount--;

        if (transitionCount < 0) {
            throw new IllegalStateException("Invalid transition stack update");
        }

        if (transitionCount == 0) {
            runPendingActions();
        }
    }

    public static boolean areTransitionsRunning() {
        ThreadUtils.assertOnUiThread();
        return (transitionCount > 0);
    }

    public static void track(PropertyAnimator animator) {
        ThreadUtils.assertOnUiThread();
        animator.addPropertyAnimationListener(propertyAnimatorListener);
    }

    public static void track(Animator animator) {
        ThreadUtils.assertOnUiThread();
        animator.addListener(animatorListener);
    }

    public static boolean cancelPendingAction(Runnable action) {
        ThreadUtils.assertOnUiThread();
        return pendingActions.removeAll(Collections.singleton(action));
    }

    public static void runAfterTransitions(Runnable action) {
        ThreadUtils.assertOnUiThread();

        if (transitionCount == 0) {
            action.run();
        } else {
            pendingActions.add(action);
        }
    }
}