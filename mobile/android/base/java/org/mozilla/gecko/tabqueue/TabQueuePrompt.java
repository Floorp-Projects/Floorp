/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.animation.TransitionsTracker;

import android.os.Bundle;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;
import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorListenerAdapter;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;
import com.nineoldandroids.view.ViewHelper;

public class TabQueuePrompt extends Locales.LocaleAwareActivity {
    public static final String LOGTAG = "Gecko" + TabQueuePrompt.class.getSimpleName();

    // Flag set during animation to prevent animation multiple-start.
    private boolean isAnimating;

    private View containerView;
    private View buttonContainer;
    private View enabledConfirmation;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showTabQueueEnablePrompt();
    }

    private void showTabQueueEnablePrompt() {
        setContentView(R.layout.tab_queue_prompt);

        final int numberOfTimesTabQueuePromptSeen = GeckoSharedPrefs.forApp(this).getInt(TabQueueHelper.PREF_TAB_QUEUE_TIMES_PROMPT_SHOWN, 0);

        findViewById(R.id.ok_button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onConfirmButtonPressed();
                Telemetry.addToHistogram("FENNEC_TABQUEUE_PROMPT_ENABLE_YES", numberOfTimesTabQueuePromptSeen);
            }
        });
        findViewById(R.id.cancel_button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.addToHistogram("FENNEC_TABQUEUE_PROMPT_ENABLE_NO", numberOfTimesTabQueuePromptSeen);
                setResult(TabQueueHelper.TAB_QUEUE_NO);
                finish();
            }
        });

        containerView = findViewById(R.id.tab_queue_container);
        buttonContainer = findViewById(R.id.button_container);
        enabledConfirmation = findViewById(R.id.enabled_confirmation);

        ViewHelper.setTranslationY(containerView, 500);
        ViewHelper.setAlpha(containerView, 0);

        final Animator translateAnimator = ObjectAnimator.ofFloat(containerView, "translationY", 0);
        translateAnimator.setDuration(400);

        final Animator alphaAnimator = ObjectAnimator.ofFloat(containerView, "alpha", 1);
        alphaAnimator.setStartDelay(200);
        alphaAnimator.setDuration(600);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(alphaAnimator, translateAnimator);
        set.setStartDelay(400);
        TransitionsTracker.track(set);

        set.start();
    }

    @Override
    public void finish() {
        super.finish();

        // Don't perform an activity-dismiss animation.
        overridePendingTransition(0, 0);
    }

    private void onConfirmButtonPressed() {
        enabledConfirmation.setVisibility(View.VISIBLE);
        ViewHelper.setAlpha(enabledConfirmation, 0);

        final Animator buttonsAlphaAnimator = ObjectAnimator.ofFloat(buttonContainer, "alpha", 0);
        buttonsAlphaAnimator.setDuration(300);

        final Animator messagesAlphaAnimator = ObjectAnimator.ofFloat(enabledConfirmation, "alpha", 1);
        messagesAlphaAnimator.setDuration(300);
        messagesAlphaAnimator.setStartDelay(200);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(buttonsAlphaAnimator, messagesAlphaAnimator);
        TransitionsTracker.track(set);

        set.addListener(new AnimatorListenerAdapter() {

            @Override
            public void onAnimationEnd(Animator animation) {

                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        slideOut();
                        setResult(TabQueueHelper.TAB_QUEUE_YES);
                    }
                }, 1000);
            }
        });

        set.start();
    }

    /**
     * Slide the overlay down off the screen and destroy it.
     */
    private void slideOut() {
        if (isAnimating) {
            return;
        }

        isAnimating = true;

        ObjectAnimator animator = ObjectAnimator.ofFloat(containerView, "translationY", containerView.getHeight());
        animator.addListener(new AnimatorListenerAdapter() {

            @Override
            public void onAnimationEnd(Animator animation) {
                finish();
            }

        });
        animator.start();
    }

    /**
     * Close the dialog if back is pressed.
     */
    @Override
    public void onBackPressed() {
        slideOut();
    }

    /**
     * Close the dialog if the anything that isn't a button is tapped.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        slideOut();
        return true;
    }
}