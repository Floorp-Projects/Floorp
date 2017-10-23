/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.toolbar;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.webapps.WebAppManifest;

public class PwaConfirm extends RelativeLayout {

    private static final String TELEMETRY_EXTRA_SHOW = "pwa_confirm_show";
    private static final String TELEMETRY_EXTRA_CANCEL = "pwa_confirm_cancel";
    private static final String TELEMETRY_EXTRA_BACK = "pwa_confirm_back";
    private static final String TELEMETRY_EXTRA_ACCEPT = "pwa_confirm_accept";
    public static final String TELEMETRY_EXTRA_ADDED = "pwa_confirm_added";


    private boolean isAnimating = false;

    public static PwaConfirm show(Context context) {

        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.ACTIONBAR, TELEMETRY_EXTRA_SHOW);

        if (context instanceof Activity) {
            final ViewGroup contetView = (ViewGroup) ((Activity) context).findViewById(R.id.gecko_layout);
            final View oldPwaConfirm = contetView.findViewById(R.id.pwa_confirm_root);
            if ((oldPwaConfirm != null)) {
                // prevent this view to be added twice.
                return (PwaConfirm) oldPwaConfirm;
            }
            View parent = LayoutInflater.from(context).inflate(R.layout.pwa_confirm, contetView);
            final PwaConfirm pwaConfirm = (PwaConfirm) parent.findViewById(R.id.pwa_confirm_root);
            pwaConfirm.appear();
            return pwaConfirm;
        }
        return null;
    }

    private void appear() {

        setAlpha(0);

        final Animator alphaAnimator = ObjectAnimator.ofFloat(this, "alpha", 1);
        alphaAnimator.setStartDelay(200);
        alphaAnimator.setDuration(300);


        alphaAnimator.start();
    }

    public void disappear() {
        if (isAnimating) {
            return;
        }

        isAnimating = true;

        setAlpha(1);

        final Animator alphaAnimator = ObjectAnimator.ofFloat(this, "alpha", 0);
        alphaAnimator.setDuration(300);

        alphaAnimator.start();
        dismiss();
    }

    public PwaConfirm(Context context) {
        super(context);
    }

    public PwaConfirm(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public PwaConfirm(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (isInEditMode()) {
            return;
        }
        init();
        setupBackpress();
    }

    private void init() {
        final OnClickListener dismiss = new OnClickListener() {
            @Override
            public void onClick(View v) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.PAGEACTION, TELEMETRY_EXTRA_CANCEL);
                disappear();
            }
        };
        final OnClickListener createShortcut = new OnClickListener() {

            @Override
            public void onClick(View v) {

                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.PAGEACTION, TELEMETRY_EXTRA_ACCEPT);

                GeckoApplication.createShortcut();
                disappear();
            }
        };
        findViewById(R.id.pwa_confirm_root).setOnClickListener(dismiss);
        findViewById(R.id.pwa_confirm_cancel).setOnClickListener(dismiss);
        findViewById(R.id.pwa_confirm_action).setOnClickListener(createShortcut);


        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab == null) {
            return;
        }
        final WebAppManifest webAppManifest = selectedTab.getWebAppManifest();

        if (webAppManifest != null) {
            ((TextView) findViewById(R.id.pwa_confirm_title)).setText(webAppManifest.getName());
            ((TextView) findViewById(R.id.pwa_confirm_url)).setText(webAppManifest.getStartUri().toString());
            ((ImageView) findViewById(R.id.pwa_confirm_icon)).setImageBitmap(webAppManifest.getIcon());
        }
    }

    private void setupBackpress() {
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.PAGEACTION, TELEMETRY_EXTRA_BACK);
                    dismiss();
                }
                return true;
            }
        });
    }

    public void dismiss() {
        ((ViewGroup) PwaConfirm.this.getParent()).removeView(PwaConfirm.this);
    }
}
