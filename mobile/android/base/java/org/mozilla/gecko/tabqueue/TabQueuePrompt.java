/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

import android.annotation.TargetApi;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Toast;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;

public class TabQueuePrompt extends Locales.LocaleAwareActivity {
    public static final String LOGTAG = "Gecko" + TabQueuePrompt.class.getSimpleName();

    private static final int SETTINGS_REQUEST_CODE = 1;

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

        final View okButton = findViewById(R.id.ok_button);
        okButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onConfirmButtonPressed();
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "tabqueue_prompt_yes");
            }
        });
        findViewById(R.id.cancel_button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "tabqueue_prompt_no");
                setResult(TabQueueHelper.TAB_QUEUE_NO);
                finish();
            }
        });
        final View settingsButton = findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onSettingsButtonPressed();
            }
        });

        final View tipView = findViewById(R.id.tip_text);
        final View settingsPermitView = findViewById(R.id.settings_permit_text);

        if (TabQueueHelper.canDrawOverlays(this)) {
            okButton.setVisibility(View.VISIBLE);
            settingsButton.setVisibility(View.GONE);
            tipView.setVisibility(View.VISIBLE);
            settingsPermitView.setVisibility(View.GONE);
        } else {
            okButton.setVisibility(View.GONE);
            settingsButton.setVisibility(View.VISIBLE);
            tipView.setVisibility(View.GONE);
            settingsPermitView.setVisibility(View.VISIBLE);
        }

        containerView = findViewById(R.id.tab_queue_container);
        buttonContainer = findViewById(R.id.button_container);
        enabledConfirmation = findViewById(R.id.enabled_confirmation);

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
        enabledConfirmation.setVisibility(View.VISIBLE);
        enabledConfirmation.setAlpha(0);

        final Animator buttonsAlphaAnimator = ObjectAnimator.ofFloat(buttonContainer, "alpha", 0);
        buttonsAlphaAnimator.setDuration(300);

        final Animator messagesAlphaAnimator = ObjectAnimator.ofFloat(enabledConfirmation, "alpha", 1);
        messagesAlphaAnimator.setDuration(300);
        messagesAlphaAnimator.setStartDelay(200);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(buttonsAlphaAnimator, messagesAlphaAnimator);

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

    @TargetApi(Build.VERSION_CODES.M)
    private void onSettingsButtonPressed() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
        intent.setData(Uri.parse("package:" + getPackageName()));
        startActivityForResult(intent, SETTINGS_REQUEST_CODE);

        Toast.makeText(this, R.string.tab_queue_prompt_permit_drawing_over_apps, Toast.LENGTH_LONG).show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != SETTINGS_REQUEST_CODE) {
            return;
        }

        if (TabQueueHelper.canDrawOverlays(this)) {
            // User granted the permission in Android's settings.
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "tabqueue_prompt_yes");

            setResult(TabQueueHelper.TAB_QUEUE_YES);
            finish();
        }
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