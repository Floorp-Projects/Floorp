/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Wes Johnston <wjohnston@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
import android.content.res.Resources;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;

import org.json.JSONArray;
import org.json.JSONObject;

public class AwesomeBar extends Activity implements GeckoEventListener {
    private static final String LOGTAG = "GeckoAwesomeBar";

    static final String URL_KEY = "url";
    static final String TITLE_KEY = "title";
    static final String CURRENT_URL_KEY = "currenturl";
    static final String TYPE_KEY = "type";
    static final String SEARCH_KEY = "search";
    static enum Type { ADD, EDIT };

    private String mType;
    private AwesomeBarTabs mAwesomeTabs;
    private AwesomeBarEditText mText;
    private ImageButton mGoButton;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(LOGTAG, "creating awesomebar");

        setContentView(R.layout.awesomebar_search);

        mAwesomeTabs = (AwesomeBarTabs) findViewById(R.id.awesomebar_tabs);
        mAwesomeTabs.setOnUrlOpenListener(new AwesomeBarTabs.OnUrlOpenListener() {
            public void onUrlOpen(String url) {
                openUrlAndFinish(url);
            }

            public void onSearch(String engine) {
                openSearchAndFinish(mText.getText().toString(), engine);
            }
        });

        mGoButton = (ImageButton) findViewById(R.id.awesomebar_button);
        mGoButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                openUrlAndFinish(mText.getText().toString());
            }
        });

        mText = (AwesomeBarEditText) findViewById(R.id.awesomebar_text);

        Resources resources = getResources();
        
        int padding[] = { mText.getPaddingLeft(),
                          mText.getPaddingTop(),
                          mText.getPaddingRight(),
                          mText.getPaddingBottom() };

        GeckoStateListDrawable states = new GeckoStateListDrawable();
        states.initializeFilter(GeckoApp.mBrowserToolbar.getHighlightColor());
        states.addState(new int[] { android.R.attr.state_focused }, resources.getDrawable(R.drawable.address_bar_url_pressed));
        states.addState(new int[] { android.R.attr.state_pressed }, resources.getDrawable(R.drawable.address_bar_url_pressed));
        states.addState(new int[] { }, resources.getDrawable(R.drawable.address_bar_url_default));
        mText.setBackgroundDrawable(states);

        mText.setPadding(padding[0], padding[1], padding[2], padding[3]);

        Intent intent = getIntent();
        String currentUrl = intent.getStringExtra(CURRENT_URL_KEY);
        mType = intent.getStringExtra(TYPE_KEY);
        if (currentUrl != null) {
            mText.setText(currentUrl);
            mText.selectAll();
        }

        mText.setOnKeyPreImeListener(new AwesomeBarEditText.OnKeyPreImeListener() {
            public boolean onKeyPreIme(View v, int keyCode, KeyEvent event) {
                InputMethodManager imm =
                        (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

                // If input method is in fullscreen mode, we want to dismiss
                // it instead of closing awesomebar straight away.
                if (!imm.isFullscreenMode() && keyCode == KeyEvent.KEYCODE_BACK) {
                    cancelAndFinish();
                    return true;
                }

                return false;
            }
        });

        mText.addTextChangedListener(new TextWatcher() {
            public void afterTextChanged(Editable s) {
                // do nothing
            }

            public void beforeTextChanged(CharSequence s, int start, int count,
                                          int after) {
                // do nothing
            }

            public void onTextChanged(CharSequence s, int start, int before,
                                      int count) {
                String text = s.toString();

                mAwesomeTabs.filter(text);
                updateGoButton(text);
            }
        });

        mText.setOnKeyListener(new View.OnKeyListener() {
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER) {
                    if (event.getAction() != KeyEvent.ACTION_DOWN)
                        return true;

                    openUrlAndFinish(mText.getText().toString());
                    return true;
                } else {
                    return false;
                }
            }
        });

        mText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            public void onFocusChange(View v, boolean hasFocus) {
                if (!hasFocus) {
                    InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                    imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                }
            }
        });

        GeckoAppShell.registerGeckoEventListener("SearchEngines:Data", this);
        GeckoAppShell.sendEventToGecko(new GeckoEvent("SearchEngines:Get", null));
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("SearchEngines:Data")) {
                mAwesomeTabs.setSearchEngines(message.getJSONArray("searchEngines"));
            }
        } catch (Exception e) {
            // do nothing
            Log.i(LOGTAG, "handleMessage throws " + e + " for message: " + event);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfiguration) {
        super.onConfigurationChanged(newConfiguration);
    }

    @Override
    public boolean onSearchRequested() {
        cancelAndFinish();
        return true;
    }

    private void updateGoButton(String text) {
        if (text.length() == 0) {
            mGoButton.setVisibility(View.GONE);
            return;
        }

        mGoButton.setVisibility(View.VISIBLE);

        int imageResource = R.drawable.ic_awesomebar_go;
        if (!GeckoAppShell.canCreateFixupURI(text)) {
            imageResource = R.drawable.ic_awesomebar_search;
        }

        mGoButton.setImageResource(imageResource);
    }

    private void cancelAndFinish() {
        setResult(Activity.RESULT_CANCELED);
        finish();
    }

    private void finishWithResult(Intent intent) {
        setResult(Activity.RESULT_OK, intent);
        finish();
        overridePendingTransition(0, 0);
    }

    private void openUrlAndFinish(String url) {
        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        resultIntent.putExtra(TYPE_KEY, mType);
        finishWithResult(resultIntent);
    }

    private void openSearchAndFinish(String url, String engine) {
        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        resultIntent.putExtra(TYPE_KEY, mType);
        resultIntent.putExtra(SEARCH_KEY, engine);
        finishWithResult(resultIntent);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // This method is called only if the key event was not handled
        // by any of the views, which usually means the edit box lost focus
        if (keyCode == KeyEvent.KEYCODE_BACK ||
            keyCode == KeyEvent.KEYCODE_MENU ||
            keyCode == KeyEvent.KEYCODE_SEARCH ||
            keyCode == KeyEvent.KEYCODE_DPAD_UP ||
            keyCode == KeyEvent.KEYCODE_DPAD_DOWN ||
            keyCode == KeyEvent.KEYCODE_DPAD_LEFT ||
            keyCode == KeyEvent.KEYCODE_DPAD_RIGHT ||
            keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
            keyCode == KeyEvent.KEYCODE_DEL) {
            return super.onKeyDown(keyCode, event);
        } else {
            int selStart = -1;
            int selEnd = -1;
            if (mText.hasSelection()) {
                selStart = mText.getSelectionStart();
                selEnd = mText.getSelectionEnd();
            }

            // Return focus to the edit box, and dispatch the event to it
            mText.requestFocusFromTouch();

            if (selStart >= 0) {
                // Restore the selection, which gets lost due to the focus switch
                mText.setSelection(selStart, selEnd);
            }

            mText.dispatchKeyEvent(event);
            return true;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mAwesomeTabs.destroy();
        GeckoAppShell.unregisterGeckoEventListener("SearchEngines:Data", this);
    }

    public static class AwesomeBarEditText extends EditText {
        OnKeyPreImeListener mOnKeyPreImeListener;

        public interface OnKeyPreImeListener {
            public boolean onKeyPreIme(View v, int keyCode, KeyEvent event);
        }

        public AwesomeBarEditText(Context context, AttributeSet attrs) {
            super(context, attrs);
            mOnKeyPreImeListener = null;
        }

        @Override
        public boolean onKeyPreIme(int keyCode, KeyEvent event) {
            if (mOnKeyPreImeListener != null)
                return mOnKeyPreImeListener.onKeyPreIme(this, keyCode, event);

            return false;
        }

        public void setOnKeyPreImeListener(OnKeyPreImeListener listener) {
            mOnKeyPreImeListener = listener;
        }
    }
}
