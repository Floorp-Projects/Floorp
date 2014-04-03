/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.PropertyAnimationListener;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.toolbar.BrowserToolbar.OnCommitListener;
import org.mozilla.gecko.toolbar.BrowserToolbar.OnDismissListener;
import org.mozilla.gecko.toolbar.BrowserToolbar.OnFilterListener;
import org.mozilla.gecko.toolbar.ToolbarEditText.OnTextTypeChangeListener;
import org.mozilla.gecko.toolbar.ToolbarEditText.TextType;
import org.mozilla.gecko.widget.ThemedLinearLayout;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageButton;

/**
* {@code ToolbarEditLayout} is the UI for when the toolbar is in
* edit state. It controls a text entry ({@code ToolbarEditText})
* and its matching 'go' button which changes depending on the
* current type of text in the entry.
*/
public class ToolbarEditLayout extends ThemedLinearLayout {

    private final ToolbarEditText mEditText;
    private final ImageButton mGo;

    private OnCommitListener mCommitListener;
    private OnFocusChangeListener mFocusChangeListener;

    public ToolbarEditLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        setOrientation(HORIZONTAL);

        LayoutInflater.from(context).inflate(R.layout.toolbar_edit_layout, this);
        mGo = (ImageButton) findViewById(R.id.go);
        mEditText = (ToolbarEditText) findViewById(R.id.url_edit_text);
    }

    @Override
    public void onAttachedToWindow() {
        mGo.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mCommitListener != null) {
                    mCommitListener.onCommit();
                }
            }
        });

        mEditText.setOnTextTypeChangeListener(new OnTextTypeChangeListener() {
            @Override
            public void onTextTypeChange(ToolbarEditText editText, TextType textType) {
                updateGoButton(textType);
            }
        });

        mEditText.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (mFocusChangeListener != null) {
                    mFocusChangeListener.onFocusChange(ToolbarEditLayout.this, hasFocus);
                }
            }
        });
    }

    @Override
    public void setOnFocusChangeListener(OnFocusChangeListener listener) {
        mFocusChangeListener = listener;
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);

        mGo.setEnabled(enabled);
        mEditText.setEnabled(enabled);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        mEditText.setPrivateMode(isPrivate);
    }

    private void updateGoButton(TextType textType) {
        if (textType == TextType.EMPTY) {
            mGo.setVisibility(View.GONE);
            return;
        }

        mGo.setVisibility(View.VISIBLE);

        final int imageResource;
        final String contentDescription;

        if (textType == TextType.SEARCH_QUERY) {
            imageResource = R.drawable.ic_url_bar_search;
            contentDescription = getContext().getString(R.string.search);
        } else {
            imageResource = R.drawable.ic_url_bar_go;
            contentDescription = getContext().getString(R.string.go);
        }

        mGo.setImageResource(imageResource);
        mGo.setContentDescription(contentDescription);
    }

    private void showSoftInput() {
        InputMethodManager imm =
               (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(mEditText, InputMethodManager.SHOW_IMPLICIT);
    }

    void prepareShowAnimation(PropertyAnimator animator) {
        if (animator == null) {
            mEditText.requestFocus();
            showSoftInput();
            return;
        }

        animator.addPropertyAnimationListener(new PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                ViewHelper.setAlpha(mGo, 0.0f);
                mEditText.requestFocus();
            }

            @Override
            public void onPropertyAnimationEnd() {
                ViewHelper.setAlpha(mGo, 1.0f);
                showSoftInput();
            }
        });
    }

    void setOnCommitListener(OnCommitListener listener) {
        mCommitListener = listener;
        mEditText.setOnCommitListener(listener);
    }

    void setOnDismissListener(OnDismissListener listener) {
        mEditText.setOnDismissListener(listener);
    }

    void setOnFilterListener(OnFilterListener listener) {
        mEditText.setOnFilterListener(listener);
    }

    boolean onKey(int keyCode, KeyEvent event) {
        final int prevSelStart = mEditText.getSelectionStart();
        final int prevSelEnd = mEditText.getSelectionEnd();

        // Manually dispatch the key event to the edit text. If selection changed as
        // a result of the key event, then give focus back to mEditText
        mEditText.dispatchKeyEvent(event);

        final int curSelStart = mEditText.getSelectionStart();
        final int curSelEnd = mEditText.getSelectionEnd();

        if (prevSelStart != curSelStart || prevSelEnd != curSelEnd) {
            mEditText.requestFocusFromTouch();

            // Restore the selection, which gets lost due to the focus switch
            mEditText.setSelection(curSelStart, curSelEnd);
        }

        return true;
    }

    void onEditSuggestion(String suggestion) {
        mEditText.setText(suggestion);
        mEditText.setSelection(mEditText.getText().length());
        mEditText.requestFocus();

        showSoftInput();
    }

    void setText(String text) {
        mEditText.setText(text);
    }

    String getText() {
        return mEditText.getText().toString();
    }
}