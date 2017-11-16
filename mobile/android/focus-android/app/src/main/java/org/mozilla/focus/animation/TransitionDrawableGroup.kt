/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.animation

import android.graphics.drawable.TransitionDrawable

/**
 * A class to allow [TransitionDrawable]'s animations to play together: similar to [android.animation.AnimatorSet].
 */
class TransitionDrawableGroup(private vararg val transitionDrawables: TransitionDrawable) {
    fun startTransition(durationMillis: Int) {
        // In theory, there are no guarantees these will play together.
        // In practice, I haven't noticed any problems.
        for (transitionDrawable in transitionDrawables) {
            transitionDrawable.startTransition(durationMillis)
        }
    }

    fun resetTransition() {
        for (transitionDrawable in transitionDrawables) {
            transitionDrawable.resetTransition()
        }
    }
}
