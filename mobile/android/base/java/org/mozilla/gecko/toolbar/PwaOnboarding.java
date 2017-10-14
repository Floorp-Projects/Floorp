/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.toolbar;

import android.app.Activity;
import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;

public class PwaOnboarding extends RelativeLayout {

    static final String LINK_PWA_SUMO = "https://developer.mozilla.org/en-US/Apps/Progressive";

    public static void show(Context context) {
        if (context instanceof Activity) {
            final ViewGroup contetView = (ViewGroup) ((Activity) context).findViewById(android.R.id.content);
            LayoutInflater.from(context).inflate(R.layout.pwa_onboarding, contetView);
        }
    }

    public PwaOnboarding(Context context) {
        super(context);
    }

    public PwaOnboarding(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public PwaOnboarding(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        init();
        setupBackpress();
    }

    private void init() {
        final OnClickListener dismiss = new OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        };
        final OnClickListener loadSumo = new OnClickListener() {

            @Override
            public void onClick(View v) {
                dismiss();
                Tabs.getInstance().loadUrlInTab(LINK_PWA_SUMO);
            }
        };
        findViewById(R.id.pwa_onboarding_root).setOnClickListener(dismiss);
        findViewById(R.id.pwa_onboarding_dismiss).setOnClickListener(dismiss);
        findViewById(R.id.pwa_onboarding_sumo).setOnClickListener(loadSumo);
    }


    private void setupBackpress() {
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
                    dismiss();
                }
                return true;
            }
        });
    }

    private void dismiss() {
        ((ViewGroup) this.getParent()).removeView(this);
    }
}
