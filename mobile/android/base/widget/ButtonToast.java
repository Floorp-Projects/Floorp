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
import android.os.Parcelable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.widget.Button;
import android.widget.TextView;

import org.mozilla.gecko.R;

public class ButtonToast {
    private final static int TOAST_DURATION = 5000;

    private View mView;
    private TextView mMessageView;
    private Button mButton;
    private ViewPropertyAnimator mBarAnimator;
    private Handler mHideHandler = new Handler();

    private ToastListener mListener;

    // State objects
    private CharSequence mToken;
    private CharSequence mButtonMessage;
    private int mButtonIcon;
    private CharSequence mMessage;

    public interface ToastListener {
        void onButtonClicked(CharSequence token);
    }

    public ButtonToast(View view, ToastListener listener) {
        mView = view;
        mBarAnimator = mView.animate();
        mListener = listener;

        mMessageView = (TextView) mView.findViewById(R.id.toast_message);
        mButton = (Button) mView.findViewById(R.id.toast_button);
        mButton.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        hide(false);
                        mListener.onButtonClicked(mToken);
                    }
                });

        hide(true);
    }

    public void show(boolean immediate, CharSequence message,
                     CharSequence buttonMessage, int buttonIcon,
                     CharSequence token) {
        mToken = token;
        mMessage = message;
        mButtonMessage = buttonMessage;
        mMessageView.setText(mMessage);

        mButton.setText(buttonMessage);
        mButton.setCompoundDrawablesWithIntrinsicBounds(0, 0, buttonIcon, 0);

        mHideHandler.removeCallbacks(mHideRunnable);
        mHideHandler.postDelayed(mHideRunnable, TOAST_DURATION);

        mView.setVisibility(View.VISIBLE);
        if (immediate) {
            mView.setAlpha(1);
        } else {
            mBarAnimator.cancel();
            mBarAnimator
                    .alpha(1)
                    .setDuration(
                            mView.getResources()
                                    .getInteger(android.R.integer.config_longAnimTime))
                    .setListener(null);
        }
    }

    public void hide(boolean immediate) {
        mHideHandler.removeCallbacks(mHideRunnable);
        if (immediate) {
            mView.setVisibility(View.GONE);
            mView.setAlpha(0);
            mMessage = null;
            mToken = null;

        } else {
            mBarAnimator.cancel();
            mBarAnimator
                    .alpha(0)
                    .setDuration(mView.getResources()
                            .getInteger(android.R.integer.config_shortAnimTime))
                    .setListener(new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            mView.setVisibility(View.GONE);
                            mMessage = null;
                            mToken = null;
                        }
                    });
        }
    }

    public void onSaveInstanceState(Bundle outState) {
        outState.putCharSequence("toast_message", mMessage);
        outState.putCharSequence("toast_button_message", mButtonMessage);
        outState.putInt("toast_button_drawable", mButtonIcon);
        outState.putCharSequence("toast_token", mToken);
    }

    public void onRestoreInstanceState(Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            mMessage = savedInstanceState.getCharSequence("toast_message");
            mButtonMessage = savedInstanceState.getCharSequence("toast_buttonmessage");
            mButtonIcon = savedInstanceState.getInt("toast_button_drawable");
            mToken = savedInstanceState.getCharSequence("toast_token");

            if (mToken != null || !TextUtils.isEmpty(mMessage)) {
                show(true, mMessage, mButtonMessage, mButtonIcon, mToken);
            }
        }
    }

    private Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            hide(false);
        }
    };
}
