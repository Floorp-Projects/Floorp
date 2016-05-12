/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.promotion;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

/**
 * Generic HelperUI (prompt) that can be populated with an image, title, message and action button.
 * See show() for usage. This is run as an Activity, results must be handled in the parent Activities
 * onActivityResult().
 */
public class SimpleHelperUI extends Locales.LocaleAwareActivity {
    public static final String PREF_FIRST_RVBP_SHOWN = "first_reader_view_bookmark_prompt_shown";
    public static final String FIRST_RVBP_SHOWN_TELEMETRYEXTRA = "first_readerview_bookmark_prompt";

    private View containerView;

    private boolean isAnimating;

    private String mTelemetryExtra;

    private static final String EXTRA_TELEMETRYEXTRA = "telemetryextra";
    private static final String EXTRA_TITLE = "title";
    private static final String EXTRA_MESSAGE = "message";
    private static final String EXTRA_IMAGE = "image";
    private static final String EXTRA_BUTTON = "button";
    private static final String EXTRA_RESULTCODE_POSITIVE = "positive";
    private static final String EXTRA_RESULTCODE_NEGATIVE = "negative";


    /**
     * Show a generic helper UI/prompt.
     *
     * @param owner The owning Activity, the result of this prompt will be delivered to its
     *              onActivityResult().
     * @param requestCode The request code for the Activity that will be created, this is passed to
     *                    onActivityResult() to identify the prompt.
     *
     * @param positiveResultCode The result code passed to onActivityResult() when the button has
     *                           been pressed.
     * @param negativeResultCode The result code passed to onActivityResult() when the prompt was
     *                           dismissed, either by pressing outside the prompt or by pressing the
     *                           device back button.
     */
    public static void show(Activity owner, String telemetryExtra,
                            int requestCode,
                            @StringRes int title, @StringRes int message,
                            @DrawableRes int image, @StringRes int buttonText,
                            int positiveResultCode, int negativeResultCode) {
        Intent intent = new Intent(owner, SimpleHelperUI.class);

        intent.putExtra(EXTRA_TELEMETRYEXTRA, telemetryExtra);

        intent.putExtra(EXTRA_TITLE, title);
        intent.putExtra(EXTRA_MESSAGE, message);

        intent.putExtra(EXTRA_IMAGE, image);
        intent.putExtra(EXTRA_BUTTON, buttonText);

        intent.putExtra(EXTRA_RESULTCODE_POSITIVE, positiveResultCode);
        intent.putExtra(EXTRA_RESULTCODE_NEGATIVE, negativeResultCode);

        owner.startActivityForResult(intent, requestCode);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mTelemetryExtra = getIntent().getStringExtra(EXTRA_TELEMETRYEXTRA);

        setupViews();

        slideIn();
    }

    private void setupViews() {
        final Intent i = getIntent();

        setContentView(R.layout.simple_helper_ui);

        containerView = findViewById(R.id.container);

        ((ImageView) findViewById(R.id.image)).setImageResource(i.getIntExtra(EXTRA_IMAGE, 0));

        ((TextView) findViewById(R.id.title)).setText(i.getIntExtra(EXTRA_TITLE, 0));

        ((TextView) findViewById(R.id.message)).setText(i.getIntExtra(EXTRA_MESSAGE, 0));

        final Button button = ((Button) findViewById(R.id.button));
        button.setText(i.getIntExtra(EXTRA_BUTTON, 0));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                slideOut();

                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, mTelemetryExtra);

                setResult(i.getIntExtra(EXTRA_RESULTCODE_POSITIVE, -1));
            }
        });
    }

    private void slideIn() {
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

    @Override
    public void finish() {
        super.finish();

        // Don't perform an activity-dismiss animation.
        overridePendingTransition(0, 0);
    }

    @Override
    public void onBackPressed() {
        slideOut();

        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.BACK, mTelemetryExtra);

        setResult(getIntent().getIntExtra(EXTRA_RESULTCODE_NEGATIVE, -1));

    }

    /**
     * User clicked outside of the prompt.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        slideOut();

        // Not really an action triggered by the "back" button but with the same effect: Finishing this
        // activity and going back to the previous one.
        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.BACK, mTelemetryExtra);

        setResult(getIntent().getIntExtra(EXTRA_RESULTCODE_NEGATIVE, -1));

        return true;
    }
}
