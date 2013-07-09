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
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.view.animation.Animation;
import android.view.animation.AlphaAnimation;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.LinkedList;

import org.mozilla.gecko.R;

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

    // State objects
    private static class Toast {
        public final CharSequence token;
        public final CharSequence buttonMessage;
        public final int buttonIcon;
        public final CharSequence message;

        public Toast(CharSequence aMessage, CharSequence aButtonMessage, int aIcon, CharSequence aToken) {
            message = aMessage;
            buttonMessage = aButtonMessage;
            buttonIcon = aIcon;
            token = aToken;
        }
    }

    public interface ToastListener {
        void onButtonClicked(CharSequence token);
    }

    public ButtonToast(View view, ToastListener listener) {
        mView = view;
        mListener = listener;

        mMessageView = (TextView) mView.findViewById(R.id.toast_message);
        mButton = (Button) mView.findViewById(R.id.toast_button);
        mButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        Toast t = mCurrentToast;
                        if (t == null)
                            return;

                        hide(false);
                        if (mListener != null) {
                            mListener.onButtonClicked(t.token);
                        }
                    }
                });

        hide(true);
    }

    public void show(boolean immediate, CharSequence message,
                     CharSequence buttonMessage, int buttonIcon,
                     CharSequence token) {
        Toast t = new Toast(message, buttonMessage, buttonIcon, token);
        show(t, immediate);
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
        mButton.setCompoundDrawablesWithIntrinsicBounds(0, 0, t.buttonIcon, 0);

        mHideHandler.removeCallbacks(mHideRunnable);
        mHideHandler.postDelayed(mHideRunnable, TOAST_DURATION);

        mView.setVisibility(View.VISIBLE);
        int duration = immediate ? 0 : mView.getResources().getInteger(android.R.integer.config_longAnimTime);

        mView.clearAnimation();
        AlphaAnimation alpha = new AlphaAnimation(0.0f, 1.0f);
        alpha.setDuration(duration);
        alpha.setFillAfter(true);
        mView.startAnimation(alpha);
    }

    public void hide(boolean immediate) {
        if (mButton.isPressed()) {
            mHideHandler.postDelayed(mHideRunnable, TOAST_DURATION);
            return;
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
            AlphaAnimation alpha = new AlphaAnimation(1.0f, 0.0f);
            alpha.setDuration(duration);
            alpha.setFillAfter(true);
            alpha.setAnimationListener(new Animation.AnimationListener () {
                // If we are showing a toast and go in the background
                // onAnimationEnd will be called when the app is restored
                public void onAnimationEnd(Animation animation) {
                    mView.setVisibility(View.GONE);
                    showNextInQueue();
                }
                public void onAnimationRepeat(Animation animation) { }
                public void onAnimationStart(Animation animation) { }
            });
            mView.startAnimation(alpha);
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
            hide(false);
        }
    };
}
