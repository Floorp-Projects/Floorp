package org.mozilla.gecko.animation;

import java.util.LinkedList;
import java.util.List;

import android.view.View;
import android.view.animation.AccelerateInterpolator;

import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;
import com.nineoldandroids.animation.ValueAnimator;

/**
 * This is an Animator that chains AccelerateInterpolators. It can be used to create a customized
 * Bounce animation.
 *
 * After constructing an instance, animations can be queued up sequentially with the
 * {@link #queue(Attributes) queue} method.
 */
public class BounceAnimatorBuilder extends ValueAnimator {

    public static final class Attributes {
        public final float value;
        public final int durationMs;

        public Attributes(float value, int duration) {
            this.value = value;
            this.durationMs = duration;
        }
    }

    private final View mView;
    private final String mPropertyName;
    private final List<Animator> animatorChain = new LinkedList<Animator>();

    public BounceAnimatorBuilder(View view, String property) {
        mView = view;
        mPropertyName = property;
    }

    public void queue(Attributes attrs) {
        final ValueAnimator animator = ObjectAnimator.ofFloat(mView, mPropertyName, attrs.value);
        animator.setDuration(attrs.durationMs);
        animator.setInterpolator(new AccelerateInterpolator());
        animatorChain.add(animator);
    }

    public AnimatorSet build(){
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playSequentially(animatorChain);
        return animatorSet;
    }
}
