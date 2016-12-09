/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.ViewFlipper;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.UrlUtils;

public class UrlBar extends ViewFlipper {
    public interface OnUrlAction {
        void onUrlEntered(String url);

        void onErase();
    }

    private OnUrlAction listener;

    private EditText urlEditView;
    private TextView urlDisplayView;

    public UrlBar(Context context) {
        super(context);

        init();
    }

    public UrlBar(Context context, AttributeSet attrs) {
        super(context, attrs);

        init();
    }

    private void init() {
        setBackgroundColor(0xFF444444);

        int result = 0;
        int resourceId = getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (resourceId > 0) {
            result = getResources().getDimensionPixelSize(resourceId);
        }
        setPadding(0, result, 0, 0);
    }

    public void onPageStarted(String url) {
        setBackgroundColor(0xFF444444);

        urlDisplayView.setText(url);
        urlEditView.setText(url);
    }

    public void onPageFinished() {
        setBackgroundResource(R.drawable.gradient_background);
    }

    public void setOnUrlEnteredListener(OnUrlAction listener) {
        this.listener = listener;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        urlEditView = (EditText) findViewById(R.id.url_edit);
        urlDisplayView = (TextView) findViewById(R.id.url);

        urlEditView.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView textView, int actionId, KeyEvent keyEvent) {
                if (actionId == EditorInfo.IME_ACTION_GO) {
                    final String url = UrlUtils.normalize(textView.getText().toString());

                    if (listener != null) {
                        listener.onUrlEntered(url);
                    }

                    urlDisplayView.setText(url);

                    showNext();
                    return true;
                }
                return false;
            }
        });

        findViewById(R.id.erase).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onErase();
                }
            }
        });
    }
}
