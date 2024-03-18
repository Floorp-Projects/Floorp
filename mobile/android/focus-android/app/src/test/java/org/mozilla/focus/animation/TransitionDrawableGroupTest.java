/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.animation;

import android.graphics.drawable.TransitionDrawable;

import org.junit.Test;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

public class TransitionDrawableGroupTest {
    @Test
    public void testStartIsCalledOnAllItems() {
        final TransitionDrawable transitionDrawable1 = mock(TransitionDrawable.class);
        final TransitionDrawable transitionDrawable2 = mock(TransitionDrawable.class);

        final TransitionDrawableGroup group = new TransitionDrawableGroup(
                transitionDrawable1, transitionDrawable2);

        group.startTransition(2500);

        verify(transitionDrawable1).startTransition(2500);
        verify(transitionDrawable2).startTransition(2500);
    }

    @Test
    public void testResetIsCalledOnAllItems() {
        final TransitionDrawable transitionDrawable1 = mock(TransitionDrawable.class);
        final TransitionDrawable transitionDrawable2 = mock(TransitionDrawable.class);

        final TransitionDrawableGroup group = new TransitionDrawableGroup(
                transitionDrawable1, transitionDrawable2);

        group.resetTransition();

        verify(transitionDrawable1).resetTransition();
        verify(transitionDrawable2).resetTransition();
    }
}
