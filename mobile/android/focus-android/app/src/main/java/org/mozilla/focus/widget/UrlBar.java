/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.net.Uri;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.ViewFlipper;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;

public class UrlBar extends ViewFlipper {
    public interface OnUrlAction {
        void onUrlEntered(String url);

        void onErase();

        void onEnteredEditMode();

        void onEditCancelled(boolean hasLoadedPage);
    }

    private OnUrlAction listener;
    private boolean hasLoadedPage;
    private EditText urlEditView;
    private TextView urlDisplayView;
    private View lockView;
    private ProgressBar progressView;

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
        hasLoadedPage = true;

        setBackgroundColor(0xFF444444);

        urlDisplayView.setText(url);
        urlEditView.setText(url);

        if (UrlUtils.isHttps(url)) {
            lockView.setVisibility(View.VISIBLE);
        } else {
            lockView.setVisibility(View.GONE);
        }

        progressView.setVisibility(View.VISIBLE);
    }

    public void onPageFinished() {
        setBackgroundResource(R.drawable.gradient_background);

        progressView.setVisibility(View.INVISIBLE);
    }

    public void setOnUrlActionListener(OnUrlAction listener) {
        this.listener = listener;
    }

    public void onProgressUpdate(int progress) {
        progressView.setProgress(progress);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        lockView = findViewById(R.id.lock);

        progressView = (ProgressBar) findViewById(R.id.progress);

        urlEditView = (EditText) findViewById(R.id.url_edit);
        urlEditView.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView textView, int actionId, KeyEvent keyEvent) {
                if (actionId == EditorInfo.IME_ACTION_GO) {
                    onUrlEntered();
                    return true;
                }
                return false;
            }
        });
        urlEditView.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus) {
                    ViewUtils.showKeyboard(urlEditView);
                }
            }
        });

        urlDisplayView = (TextView) findViewById(R.id.url);
        urlDisplayView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onEnteredEditMode();
                }

                setBackgroundColor(0xFF444444);

                showPrevious();

                urlEditView.requestFocus();
            }
        });

        findViewById(R.id.cancel).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onEditCancelled(hasLoadedPage);
                }

                if (hasLoadedPage) {
                    showNext();

                    setBackgroundResource(R.drawable.gradient_background);
                }
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

    private void onUrlEntered() {
        ViewUtils.hideKeyboard(urlEditView);

        final String rawUrl = urlEditView.getText().toString();

        final String url = UrlUtils.isUrl(rawUrl)
                ? UrlUtils.normalize(rawUrl)
                : UrlUtils.createSearchUrl(rawUrl);

        if (listener != null) {
            listener.onUrlEntered(url);
        }

        urlDisplayView.setText(url);

        showNext();
    }

    public void enterUrl(String url) {
        urlEditView.setText(url);
    }
}
