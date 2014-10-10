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

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;

import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class ButtonToast {
    @SuppressWarnings("unused")
    private final static String LOGTAG = "GeckoButtonToast";

    public static int LENGTH_SHORT = 3000;
    public static int LENGTH_LONG = 5000;

    private final TextView mMessageView;
    private final Button mButton;
    private final Handler mHideHandler = new Handler();
    final View mView;
    Toast mCurrentToast;

    public enum ReasonHidden {
        CLICKED,
        TOUCH_OUTSIDE,
        TIMEOUT,
        REPLACED,
        STARTUP
    }

    // State objects
    private static class Toast {
        public final CharSequence buttonMessage;
        public Drawable buttonDrawable;
        public final CharSequence message;
        public final int duration;
        public ToastListener listener;

        public Toast(CharSequence aMessage, int aDuration,
                CharSequence aButtonMessage, Drawable aDrawable,
                ToastListener aListener) {
            message = aMessage;
            duration = aDuration;
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
                     int duration, CharSequence buttonMessage,
                     int buttonDrawableId, ToastListener listener) {
        final Drawable d = mView.getContext().getResources().getDrawable(buttonDrawableId);
        show(false, message, duration, buttonMessage, d, listener);
    }

    public void show(boolean immediate, CharSequence message,
                     int duration, CharSequence buttonMessage,
                     Drawable buttonDrawable, ToastListener listener) {
        show(new Toast(message, duration, buttonMessage, buttonDrawable, listener), immediate);
    }

    private void show(Toast t, boolean immediate) {
        // If we're already showing a toast, replace it with the new one by hiding the old one and quickly showing the new one.
        if (mCurrentToast != null) {
            hide(true, ReasonHidden.REPLACED);
            immediate = true;
        }

        mCurrentToast = t;
        mButton.setEnabled(true);

        // Our toast is re-used, so we update all fields to clear any old values.
        mMessageView.setText(null != t.message ? t.message : "");
        mButton.setText(null != t.buttonMessage ? t.buttonMessage : "");
        mButton.setCompoundDrawablesWithIntrinsicBounds(t.buttonDrawable, null, null, null);

        mHideHandler.removeCallbacks(mHideRunnable);
        mHideHandler.postDelayed(mHideRunnable, t.duration);

        mView.setVisibility(View.VISIBLE);
        int duration = immediate ? 0 : mView.getResources().getInteger(android.R.integer.config_longAnimTime);

        PropertyAnimator animator = new PropertyAnimator(duration);
        animator.attach(mView, PropertyAnimator.Property.ALPHA, 1.0f);
        animator.start();
    }

    public void hide(boolean immediate, ReasonHidden reason) {
        // There's nothing to do if the view is already hidden.
        if (mView.getVisibility() == View.GONE) {
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
        } else {
            // Using Android's animation frameworks will not correctly turn off clicking.
            // See bug 885717.
            PropertyAnimator animator = new PropertyAnimator(duration);
            animator.attach(mView, PropertyAnimator.Property.ALPHA, 0.0f);
            animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener () {
                // If we are showing a toast and go in the background
                // onAnimationEnd will be called when the app is restored
                @Override
                public void onPropertyAnimationEnd() {
                    mView.clearAnimation();
                    mView.setVisibility(View.GONE);
                }
                @Override
                public void onPropertyAnimationStart() { }
            });
            animator.start();
        }
    }

    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            hide(false, ReasonHidden.TIMEOUT);
        }
    };
}
