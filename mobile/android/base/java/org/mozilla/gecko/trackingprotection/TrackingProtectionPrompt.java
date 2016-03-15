/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.trackingprotection;

import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Intent;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;

public class TrackingProtectionPrompt extends Locales.LocaleAwareActivity {
        public static final String LOGTAG = "Gecko" + TrackingProtectionPrompt.class.getSimpleName();

        // Flag set during animation to prevent animation multiple-start.
        private boolean isAnimating;

        private View containerView;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            showPrompt();
        }

        private void showPrompt() {
            setContentView(R.layout.tracking_protection_prompt);

            findViewById(R.id.ok_button).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    onConfirmButtonPressed();
                }
            });
            findViewById(R.id.link_text).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    slideOut();
                    final Intent settingsIntent = new Intent(TrackingProtectionPrompt.this, GeckoPreferences.class);
                    GeckoPreferences.setResourceToOpen(settingsIntent, "preferences_privacy");
                    startActivity(settingsIntent);

                    // Don't use a transition to settings if we're on a device where that
                    // would look bad.
                    if (HardwareUtils.IS_KINDLE_DEVICE) {
                        overridePendingTransition(0, 0);
                    }
                }
            });

            containerView = findViewById(R.id.tracking_protection_inner_container);

            containerView.setTranslationY(500);
            containerView.setAlpha(0);

            final Animator translateAnimator = ObjectAnimator.ofFloat(containerView, "translationY", 0);
            translateAnimator.setDuration(400);

            final Animator alphaAnimator = ObjectAnimator.ofFloat(containerView, "alpha", 1);
            alphaAnimator.setStartDelay(200);
            alphaAnimator.setDuration(600);

            final AnimatorSet set = new AnimatorSet();
            set.playTogether(alphaAnimator, translateAnimator);
            set.setStartDelay(400);

            set.start();
        }

        @Override
        public void finish() {
            super.finish();

            // Don't perform an activity-dismiss animation.
            overridePendingTransition(0, 0);
        }

        private void onConfirmButtonPressed() {
            slideOut();
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
