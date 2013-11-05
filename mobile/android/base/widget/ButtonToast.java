/*
 * Copyright 2012 Roman Nurik
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.gecko.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.view.animation.Animation;
import android.view.animation.AlphaAnimation;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.LinkedList;

import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.R;
import org.mozilla.gecko.gfx.BitmapUtils;

public class ButtonToast {
    private final static String LOGTAG = "GeckoButtonToast";
    private final static int TOAST_DURATION = 5000;

    private final View mView;
    private final TextView mMessageView;
    private final Button mButton;
    private final Handler mHideHandler = new Handler();

    private final ToastListener mListener;
    private final LinkedList<Toast> mQueue = new LinkedList<Toast>();
    private Toast mCurrentToast;

    public enum ReasonHidden {
        CLICKED,
        TIMEOUT,
        STARTUP
    }

    // State objects
    private static class Toast {
        public final CharSequence buttonMessage;
        public Drawable buttonDrawable;
        public final CharSequence message;
        public ToastListener listener;

        public Toast(CharSequence aMessage, CharSequence aButtonMessage,
                     Drawable aDrawable, ToastListener aListener) {
            message = aMessage;
            buttonMessage = aButtonMessage;
            buttonDrawable = aDrawable;
            listener = aListener;
        }
    }

    public interface ToastListener {
        void onButtonClicked();
        void onToastHidden(ReasonHidden reason);
    }

    public ButtonToast(View view) {
        mView = view;
        mListener = null;
        mMessageView = (TextView) mView.findViewById(R.id.toast_message);
        mButton = (Button) mView.findViewById(R.id.toast_button);
        mButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        Toast t = mCurrentToast;
                        if (t == null)
                            return;

                        hide(false, ReasonHidden.CLICKED);
                        if (t.listener != null) {
                            t.listener.onButtonClicked();
                        }
                    }
                });

        hide(true, ReasonHidden.STARTUP);
    }

    public void show(boolean immediate, CharSequence message,
                     CharSequence buttonMessage, Drawable buttonDrawable,
                     ToastListener listener) {
        show(new Toast(message, buttonMessage, buttonDrawable, listener), immediate);
    }

    private void show(Toast t, boolean immediate) {
        // If we're already showing a toast, add this one to the queue to show later
        if (mView.getVisibility() == View.VISIBLE) {
            mQueue.offer(t);
            return;
        }

        mCurrentToast = t;
        mButton.setEnabled(true);

        mMessageView.setText(t.message);
        mButton.setText(t.buttonMessage);
        mButton.setCompoundDrawablePadding(mView.getContext().getResources().getDimensionPixelSize(R.dimen.toast_button_padding));
        mButton.setCompoundDrawablesWithIntrinsicBounds(t.buttonDrawable, null, null, null);

        mHideHandler.removeCallbacks(mHideRunnable);
        mHideHandler.postDelayed(mHideRunnable, TOAST_DURATION);

        mView.setVisibility(View.VISIBLE);
        int duration = immediate ? 0 : mView.getResources().getInteger(android.R.integer.config_longAnimTime);

        PropertyAnimator animator = new PropertyAnimator(duration);
        animator.attach(mView, PropertyAnimator.Property.ALPHA, 1.0f);
        animator.start();
    }

    public void hide(boolean immediate, ReasonHidden reason) {
        if (mButton.isPressed() && reason != ReasonHidden.CLICKED) {
            mHideHandler.postDelayed(mHideRunnable, TOAST_DURATION);
            return;
        }

        if (mCurrentToast != null && mCurrentToast.listener != null) {
            mCurrentToast.listener.onToastHidden(reason);
        }
        mCurrentToast = null;
        mButton.setEnabled(false);
        mHideHandler.removeCallbacks(mHideRunnable);
        int duration = immediate ? 0 : mView.getResources().getInteger(android.R.integer.config_longAnimTime);

        mView.clearAnimation();
        if (immediate) {
            mView.setVisibility(View.GONE);
            showNextInQueue();
        } else {
            // Using Android's animation frameworks will not correctly turn off clicking.
            // See bug 885717.
            PropertyAnimator animator = new PropertyAnimator(duration);
            animator.attach(mView, PropertyAnimator.Property.ALPHA, 0.0f);
            animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener () {
                // If we are showing a toast and go in the background
                // onAnimationEnd will be called when the app is restored
                public void onPropertyAnimationEnd() {
                    mView.setVisibility(View.GONE);
                    showNextInQueue();
                }
                public void onPropertyAnimationStart() { }
            });
            animator.start();
        }
    }

    public void onSaveInstanceState(Bundle outState) {
        // Add whatever toast we're currently showing to the front of the queue
        if (mCurrentToast != null) {
            mQueue.add(0, mCurrentToast);
        }
    }

    private void showNextInQueue() {
        Toast t = mQueue.poll();
        if (t != null) {
            show(t, false);
        }
    }

    private Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            hide(false, ReasonHidden.TIMEOUT);
        }
    };
}
