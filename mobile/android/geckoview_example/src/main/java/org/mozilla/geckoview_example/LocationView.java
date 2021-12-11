/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.TextView;
import androidx.appcompat.widget.AppCompatEditText;

public class LocationView extends AppCompatEditText {

  private CommitListener mCommitListener;
  private FocusAndCommitListener mFocusCommitListener = new FocusAndCommitListener();

  public interface CommitListener {
    void onCommit(String text);
  }

  public LocationView(Context context) {
    super(context);

    this.setInputType(EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_URI);
    this.setSingleLine(true);
    this.setSelectAllOnFocus(true);
    this.setHint(R.string.location_hint);

    setOnFocusChangeListener(mFocusCommitListener);
    setOnEditorActionListener(mFocusCommitListener);
  }

  public void setCommitListener(CommitListener listener) {
    mCommitListener = listener;
  }

  private class FocusAndCommitListener implements OnFocusChangeListener, OnEditorActionListener {
    private String mInitialText;
    private boolean mCommitted;

    @Override
    public void onFocusChange(View view, boolean focused) {
      if (focused) {
        mInitialText = ((TextView) view).getText().toString();
        mCommitted = false;
      } else if (!mCommitted) {
        setText(mInitialText);
      }
    }

    @Override
    public boolean onEditorAction(TextView textView, int i, KeyEvent keyEvent) {
      if (mCommitListener != null) {
        mCommitListener.onCommit(textView.getText().toString());
      }

      mCommitted = true;
      return true;
    }
  }
}
