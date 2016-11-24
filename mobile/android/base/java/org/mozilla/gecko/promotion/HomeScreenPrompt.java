/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.promotion;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * Prompt to promote adding the current website to the home screen.
 */
public class HomeScreenPrompt extends Locales.LocaleAwareActivity implements IconCallback {
    private static final String EXTRA_TITLE = "title";
    private static final String EXTRA_URL = "url";

    private static final String TELEMETRY_EXTRA = "home_screen_promotion";

    private View containerView;
    private ImageView iconView;
    private String title;
    private String url;
    private boolean isAnimating;
    private boolean hasAccepted;
    private boolean hasDeclined;

    public static void show(Context context, String url, String title) {
        Intent intent = new Intent(context, HomeScreenPrompt.class);
        intent.putExtra(EXTRA_TITLE, title);
        intent.putExtra(EXTRA_URL, url);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        fetchDataFromIntent();
        setupViews();
        loadShortcutIcon();

        slideIn();

        Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, Experiments.PROMOTE_ADD_TO_HOMESCREEN);

        // Technically this isn't triggered by a "service". But it's also triggered by a background task and without
        // actual user interaction.
        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.SERVICE, TELEMETRY_EXTRA);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, Experiments.PROMOTE_ADD_TO_HOMESCREEN);
    }

    private void fetchDataFromIntent() {
        final Bundle extras = getIntent().getExtras();

        title = extras.getString(EXTRA_TITLE);
        url = extras.getString(EXTRA_URL);
    }

    private void setupViews() {
        setContentView(R.layout.homescreen_prompt);

        ((TextView) findViewById(R.id.title)).setText(title);

        Uri uri = Uri.parse(url);
        ((TextView) findViewById(R.id.host)).setText(uri.getHost());

        containerView = findViewById(R.id.container);
        iconView = (ImageView) findViewById(R.id.icon);

        findViewById(R.id.add).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                hasAccepted = true;

                addToHomeScreen();
            }
        });

        findViewById(R.id.close).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onDecline();
            }
        });
    }

    private void addToHomeScreen() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                GeckoAppShell.createShortcut(title, url);

                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, TELEMETRY_EXTRA);

                ActivityUtils.goToHomeScreen(HomeScreenPrompt.this);

                finish();
            }
        });
    }



    private void loadShortcutIcon() {
        Icons.with(this)
                .pageUrl(url)
                .skipNetwork()
                .skipMemory()
                .forLauncherIcon()
                .build()
                .execute(this);
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

    /**
     * Remember that the user rejected creating a home screen shortcut for this URL.
     */
    private void rememberRejection() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final UrlAnnotations urlAnnotations = BrowserDB.from(HomeScreenPrompt.this).getUrlAnnotations();
                urlAnnotations.insertHomeScreenShortcut(getContentResolver(), url, false);
            }
        });
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
        onDecline();
    }

    private void onDecline() {
        if (hasDeclined || hasAccepted) {
            return;
        }

        rememberRejection();
        slideOut();

        // Technically not always an action triggered by the "back" button but with the same effect: Finishing this
        // activity and going back to the previous one.
        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.BACK, TELEMETRY_EXTRA);

        hasDeclined = true;
    }

    /**
     * User clicked outside of the prompt.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        onDecline();

        return true;
    }

    @Override
    public void onIconResponse(IconResponse response) {
        iconView.setImageBitmap(response.getBitmap());
    }
}
