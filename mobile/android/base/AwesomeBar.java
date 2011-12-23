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
import android.app.ActionBar;
import android.content.Intent;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.EditorInfo;
import android.widget.AdapterView;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.widget.ListView;

import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.db.BrowserDB;

import org.json.JSONArray;
import org.json.JSONObject;

public class AwesomeBar extends Activity implements GeckoEventListener {
    private static final String LOGTAG = "GeckoAwesomeBar";

    static final String URL_KEY = "url";
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

        if (Build.VERSION.SDK_INT >= 11) {
            RelativeLayout actionBarLayout = (RelativeLayout) getLayoutInflater().inflate(R.layout.awesomebar_search_actionbar, null);

            GeckoActionBar.setBackgroundDrawable(this, getResources().getDrawable(R.drawable.gecko_actionbar_bg));
            GeckoActionBar.setDisplayOptions(this, ActionBar.DISPLAY_SHOW_CUSTOM, ActionBar.DISPLAY_SHOW_CUSTOM |
                                                                                  ActionBar.DISPLAY_SHOW_HOME |
                                                                                  ActionBar.DISPLAY_SHOW_TITLE |
                                                                                  ActionBar.DISPLAY_USE_LOGO);
            GeckoActionBar.setCustomView(this, actionBarLayout);

            mGoButton = (ImageButton) actionBarLayout.findViewById(R.id.awesomebar_button);
            mText = (AwesomeBarEditText) actionBarLayout.findViewById(R.id.awesomebar_text);
        } else {
            mGoButton = (ImageButton) findViewById(R.id.awesomebar_button);
            mText = (AwesomeBarEditText) findViewById(R.id.awesomebar_text);
        }

        mAwesomeTabs = (AwesomeBarTabs) findViewById(R.id.awesomebar_tabs);
        mAwesomeTabs.setOnUrlOpenListener(new AwesomeBarTabs.OnUrlOpenListener() {
            public void onUrlOpen(String url) {
                submitAndFinish(url);
            }

            public void onSearch(String engine) {
                openSearchAndFinish(mText.getText().toString(), engine);
            }
        });

        mGoButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                submitAndFinish(mText.getText().toString());
            }
        });

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

                if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_DOWN) {
                    submitAndFinish(mText.getText().toString());
                    return true;
                }

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

                    submitAndFinish(mText.getText().toString());
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

        registerForContextMenu(mAwesomeTabs.findViewById(R.id.all_pages_list));
        registerForContextMenu(mAwesomeTabs.findViewById(R.id.bookmarks_list));
        registerForContextMenu(mAwesomeTabs.findViewById(R.id.history_list));

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

    /*
     * This method tries to guess if the given string could be a search query or URL
     * Search examples:
     *  foo
     *  foo bar.com
     *  foo http://bar.com
     *
     * URL examples
     *  foo.com
     *  foo.c
     *  :foo
     *  http://foo.com bar
    */
    private boolean isSearchUrl(String text) {
        text = text.trim();
        if (text.length() == 0)
            return false;

        int colon = text.indexOf(':');
        int dot = text.indexOf('.');
        int space = text.indexOf(' ');

        // If a space is found before any dot or colon, we assume this is a search query
        boolean spacedOut = space > -1 && (space < colon || space < dot);

        return spacedOut || (dot == -1 && colon == -1);
    }

    private void updateGoButton(String text) {
        if (text.length() == 0) {
            mGoButton.setVisibility(View.GONE);
            return;
        }

        mGoButton.setVisibility(View.VISIBLE);

        int imageResource = R.drawable.ic_awesomebar_go;
        int imeAction = EditorInfo.IME_ACTION_GO;
        if (isSearchUrl(text)) {
            imageResource = R.drawable.ic_awesomebar_search;
            imeAction = EditorInfo.IME_ACTION_SEARCH;
        }
        mGoButton.setImageResource(imageResource);

        if ((mText.getImeOptions() & EditorInfo.IME_MASK_ACTION) != imeAction) {
            InputMethodManager imm = (InputMethodManager) mText.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            mText.setImeOptions(imeAction);
            imm.restartInput(mText);
        }
    }

    private void submitAndFinish(String url) {
        if (isSearchUrl(url))
            openSearchAndFinish(url, "__default__");
        else
            openUrlAndFinish(url);
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
            keyCode == KeyEvent.KEYCODE_DEL ||
            keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
            keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
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
    public void onResume() {
        super.onResume();
        if (mText != null && mText.getText() != null)
            updateGoButton(mText.getText().toString());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mAwesomeTabs.destroy();
        GeckoAppShell.unregisterGeckoEventListener("SearchEngines:Data", this);
    }

    private Cursor mContextMenuCursor = null;

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, view, menuInfo);

        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        ListView list = (ListView) view;
        Object selecteditem = list.getItemAtPosition(info.position);

        if (!(selecteditem instanceof Cursor)) {
            mContextMenuCursor = null;
            return;
        }

        mContextMenuCursor = (Cursor) selecteditem;

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.awesomebar_contextmenu, menu);

        String title = mContextMenuCursor.getString(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.TITLE));
        menu.setHeaderTitle(title);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        if (mContextMenuCursor == null)
            return false;

        AdapterContextMenuInfo info = (AdapterContextMenuInfo) item.getMenuInfo();

        switch (item.getItemId()) {
            case R.id.open_new_tab: {
                String url = mContextMenuCursor.getString(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.URL));
                GeckoApp.mAppContext.loadUrl(url, AwesomeBar.Type.ADD);
                break;
            }
            case R.id.add_to_launcher: {
                String url = mContextMenuCursor.getString(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.URL));
                byte[] b = (byte[]) mContextMenuCursor.getBlob(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.FAVICON));
                String title = mContextMenuCursor.getString(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.TITLE));
    
                Bitmap bitmap = null;
                if (b != null)
                    bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
    
                GeckoAppShell.createShortcut(title, url, bitmap, "");
                break;
            }
            case R.id.share: {
                String url = mContextMenuCursor.getString(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.URL));
                String title = mContextMenuCursor.getString(mContextMenuCursor.getColumnIndexOrThrow(URLColumns.TITLE));
                GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                              Intent.ACTION_SEND, title);
                break;
            }
            default: {
                mContextMenuCursor = null;
                return super.onContextItemSelected(item);
            }
        }
        mContextMenuCursor = null;
        return true;
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
