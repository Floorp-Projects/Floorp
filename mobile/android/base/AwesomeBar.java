/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.Editable;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.EditorInfo;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ExpandableListView;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TabWidget;
import android.widget.Toast;

import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Map;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.db.BrowserDB;

import org.json.JSONObject;

public class AwesomeBar extends GeckoActivity implements GeckoEventListener {
    private static final String LOGTAG = "GeckoAwesomeBar";

    private static final int SUGGESTION_TIMEOUT = 2000;
    private static final int SUGGESTION_MAX = 3;

    static final String URL_KEY = "url";
    static final String CURRENT_URL_KEY = "currenturl";
    static final String TARGET_KEY = "target";
    static final String SEARCH_KEY = "search";
    static final String USER_ENTERED_KEY = "user_entered";
    static enum Target { NEW_TAB, CURRENT_TAB };

    private String mTarget;
    private AwesomeBarTabs mAwesomeTabs;
    private AwesomeBarEditText mText;
    private ImageButton mGoButton;
    private ContentResolver mResolver;
    private ContextMenuSubject mContextMenuSubject;
    private SuggestClient mSuggestClient;
    private AsyncTask<String, Void, ArrayList<String>> mSuggestTask;

    private static String sSuggestEngine;
    private static String sSuggestTemplate;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(LOGTAG, "creating awesomebar");

        mResolver = Tabs.getInstance().getContentResolver();
        LayoutInflater.from(this).setFactory(GeckoViewsFactory.getInstance());

        setContentView(R.layout.awesomebar);

        mGoButton = (ImageButton) findViewById(R.id.awesomebar_button);
        mText = (AwesomeBarEditText) findViewById(R.id.awesomebar_text);

        TabWidget tabWidget = (TabWidget) findViewById(android.R.id.tabs);
        tabWidget.setDividerDrawable(null);

        mAwesomeTabs = (AwesomeBarTabs) findViewById(R.id.awesomebar_tabs);
        mAwesomeTabs.setOnUrlOpenListener(new AwesomeBarTabs.OnUrlOpenListener() {
            public void onUrlOpen(String url) {
                openUrlAndFinish(url);
            }

            public void onSearch(String engine, String text) {
                openSearchAndFinish(text, engine);
            }

            public void onEditSuggestion(final String text) {
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mText.setText(text);
                        mText.setSelection(mText.getText().length());
                        mText.requestFocus();
                        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.showSoftInput(mText, InputMethodManager.SHOW_IMPLICIT);
                    }
                });
            }
        });

        mGoButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                openUserEnteredAndFinish(mText.getText().toString());
            }
        });

        Intent intent = getIntent();
        String currentUrl = intent.getStringExtra(CURRENT_URL_KEY);
        mTarget = intent.getStringExtra(TARGET_KEY);
        if (currentUrl != null) {
            mText.setText(currentUrl);
            mText.selectAll();
        }

        mText.setOnKeyPreImeListener(new AwesomeBarEditText.OnKeyPreImeListener() {
            public boolean onKeyPreIme(View v, int keyCode, KeyEvent event) {
                // We only want to process one event per tap
                if (event.getAction() != KeyEvent.ACTION_DOWN)
                    return false;

                if (keyCode == KeyEvent.KEYCODE_ENTER) {
                    // If the AwesomeBar has a composition string, don't submit the text yet.
                    // ENTER is needed to commit the composition string.
                    Editable content = mText.getText();
                    if (!hasCompositionString(content)) {
                        openUserEnteredAndFinish(content.toString());
                        return true;
                    }
                }

                // If input method is in fullscreen mode, we want to dismiss
                // it instead of closing awesomebar straight away.
                InputMethodManager imm =
                        (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (keyCode == KeyEvent.KEYCODE_BACK && !imm.isFullscreenMode()) {
                    // Let mAwesomeTabs try to handle the back press, since we may be in a
                    // bookmarks sub-folder.
                    if (mAwesomeTabs.onBackPressed())
                        return true;

                    // If mAwesomeTabs.onBackPressed() returned false, we didn't move up
                    // a folder level, so just exit the activity.
                    cancelAndFinish();
                    return true;
                }

                return false;
            }
        });

        mText.addTextChangedListener(new TextWatcher() {
            public void afterTextChanged(Editable s) {
                String text = s.toString();
                mAwesomeTabs.filter(text);

                // If the AwesomeBar has a composition string, don't call updateGoButton().
                // That method resets IME and composition state will be broken.
                if (hasCompositionString(s)) {
                    return;
                }

                // no composition string. It is safe to update IME flags.
                updateGoButton(text);

                // cancel previous query
                if (mSuggestTask != null) {
                    mSuggestTask.cancel(true);
                }

                if (mSuggestClient != null) {
                    mSuggestTask = new AsyncTask<String, Void, ArrayList<String>>() {
                         protected ArrayList<String> doInBackground(String... query) {
                             return mSuggestClient.query(query[0]);
                         }

                         protected void onPostExecute(ArrayList<String> suggestions) {
                             mAwesomeTabs.setSuggestions(suggestions);
                         }
                    };
                    mSuggestTask.execute(text);
                }
            }

            public void beforeTextChanged(CharSequence s, int start, int count,
                                          int after) {
                // do nothing
            }

            public void onTextChanged(CharSequence s, int start, int before,
                                      int count) {
                // do nothing
            }
        });

        mText.setOnKeyListener(new View.OnKeyListener() {
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER) {
                    if (event.getAction() != KeyEvent.ACTION_DOWN)
                        return true;

                    openUserEnteredAndFinish(mText.getText().toString());
                    return true;
                } else {
                    return false;
                }
            }
        });

        registerForContextMenu(mAwesomeTabs.findViewById(R.id.all_pages_list));
        registerForContextMenu(mAwesomeTabs.findViewById(R.id.bookmarks_list));
        registerForContextMenu(mAwesomeTabs.findViewById(R.id.history_list));

        if (sSuggestTemplate == null) {
            loadSuggestClientFromPrefs();
        } else {
            loadSuggestClient();
        }

        GeckoAppShell.registerGeckoEventListener("SearchEngines:Data", this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:Get", null));
    }

    private void loadSuggestClientFromPrefs() {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                SharedPreferences prefs = getSearchPreferences();
                sSuggestEngine = prefs.getString("suggestEngine", null);
                sSuggestTemplate = prefs.getString("suggestTemplate", null);
                if (sSuggestTemplate != null) {
                    loadSuggestClient();
                    mAwesomeTabs.setSuggestEngine(sSuggestEngine, null);
                }
            }
        });
    }

    private void loadSuggestClient() {
        mSuggestClient = new SuggestClient(GeckoApp.mAppContext, sSuggestTemplate, SUGGESTION_TIMEOUT, SUGGESTION_MAX);
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("SearchEngines:Data")) {
                final String suggestEngine = message.optString("suggestEngine");
                final String suggestTemplate = message.optString("suggestTemplate");
                if (!TextUtils.equals(suggestTemplate, sSuggestTemplate)) {
                    saveSuggestEngineData(suggestEngine, suggestTemplate);
                    sSuggestEngine = suggestEngine;
                    sSuggestTemplate = suggestTemplate;
                    loadSuggestClient();
                }
                mAwesomeTabs.setSearchEngines(suggestEngine, message.getJSONArray("searchEngines"));
            }
        } catch (Exception e) {
            // do nothing
            Log.i(LOGTAG, "handleMessage throws " + e + " for message: " + event);
        }
    }

    private void saveSuggestEngineData(final String suggestEngine, final String suggestTemplate) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                SharedPreferences prefs = getSearchPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putString("suggestEngine", suggestEngine);
                editor.putString("suggestTemplate", suggestTemplate);
                editor.commit();
            }
        });
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

        int actionBits = mText.getImeOptions() & EditorInfo.IME_MASK_ACTION;
        if (actionBits != imeAction) {
            InputMethodManager imm = (InputMethodManager) mText.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            int optionBits = mText.getImeOptions() & ~EditorInfo.IME_MASK_ACTION;
            mText.setImeOptions(optionBits | imeAction);
            imm.restartInput(mText);
        }
    }

    private void cancelAndFinish() {
        setResult(Activity.RESULT_CANCELED);
        finish();
        overridePendingTransition(0, 0);
    }

    private void finishWithResult(Intent intent) {
        setResult(Activity.RESULT_OK, intent);
        finish();
        overridePendingTransition(0, 0);
    }

    private void openUrlAndFinish(String url) {
        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        resultIntent.putExtra(TARGET_KEY, mTarget);
        finishWithResult(resultIntent);
    }

    private void openUserEnteredAndFinish(String url) {
        int index = url.indexOf(' ');
        if (index != -1) {
            String keywordUrl = BrowserDB.getUrlForKeyword(mResolver, url.substring(0, index));
            if (keywordUrl != null && keywordUrl.contains("%s")) {
                String search = URLEncoder.encode(url.substring(index + 1));
                url = keywordUrl.replace("%s", search);
            }
        }

        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        resultIntent.putExtra(TARGET_KEY, mTarget);
        resultIntent.putExtra(USER_ENTERED_KEY, true);
        finishWithResult(resultIntent);
    }

    private void openSearchAndFinish(String url, String engine) {
        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        resultIntent.putExtra(TARGET_KEY, mTarget);
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

            if (selStart >= 0) {
                // Restore the selection, which gets lost due to the focus switch
                mText.setSelection(selStart, selEnd);
            }

            // Manually dispatch the key event to the AwesomeBar before restoring (default) input
            // focus. dispatchKeyEvent() will update AwesomeBar's cursor position.
            mText.dispatchKeyEvent(event);
            int newCursorPos = mText.getSelectionEnd();

            // requestFocusFromTouch() will select all AwesomeBar text, so we must restore cursor
            // position so subsequent typing does not overwrite all text.
            mText.requestFocusFromTouch();
            mText.setSelection(newCursorPos);

            return true;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mText != null && mText.getText() != null)
            updateGoButton(mText.getText().toString());

        // Invlidate the cached value that keeps track of whether or
        // not desktop bookmarks exist
        BrowserDB.invalidateCachedState();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mAwesomeTabs.destroy();
        GeckoAppShell.unregisterGeckoEventListener("SearchEngines:Data", this);
    }

    @Override
    public void onBackPressed() {
        // Let mAwesomeTabs try to handle the back press, since we may be in a
        // bookmarks sub-folder.
        if (mAwesomeTabs.onBackPressed())
            return;

        // Otherwise, just exit the awesome screen
        cancelAndFinish();
    }

    private class ContextMenuSubject {
        public int id;
        public String url;
        public byte[] favicon;
        public String title;
        public String keyword;

        public ContextMenuSubject(int id, String url, byte[] favicon, String title, String keyword) {
            this.id = id;
            this.url = url;
            this.favicon = favicon;
            this.title = title;
            this.keyword = keyword;
        }
    };

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, view, menuInfo);
        ListView list = (ListView) view;
        mContextMenuSubject = null;

        if (list == findViewById(R.id.history_list)) {
            if (!(menuInfo instanceof ExpandableListView.ExpandableListContextMenuInfo)) {
                Log.e(LOGTAG, "menuInfo is not ExpandableListContextMenuInfo");
                return;
            }

            ExpandableListView.ExpandableListContextMenuInfo info = (ExpandableListView.ExpandableListContextMenuInfo) menuInfo;
            int childPosition = ExpandableListView.getPackedPositionChild(info.packedPosition);
            int groupPosition = ExpandableListView.getPackedPositionGroup(info.packedPosition);

            // Check if long tap is on a header row
            if (groupPosition < 0 || childPosition < 0)
                return;

            ExpandableListView exList = (ExpandableListView) list;

            // The history list is backed by a SimpleExpandableListAdapter
            @SuppressWarnings("rawtypes")
            Map map = (Map) exList.getExpandableListAdapter().getChild(groupPosition, childPosition);
            mContextMenuSubject = new ContextMenuSubject((Integer) map.get(Combined.HISTORY_ID),
                                                         (String) map.get(URLColumns.URL),
                                                         (byte[]) map.get(URLColumns.FAVICON),
                                                         (String) map.get(URLColumns.TITLE),
                                                         null);
        } else {
            if (!(menuInfo instanceof AdapterView.AdapterContextMenuInfo)) {
                Log.e(LOGTAG, "menuInfo is not AdapterContextMenuInfo");
                return;
            }

            AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
            Object selectedItem = list.getItemAtPosition(info.position);

            if (!(selectedItem instanceof Cursor)) {
                Log.e(LOGTAG, "item at " + info.position + " is not a Cursor");
                return;
            }

            Cursor cursor = (Cursor) selectedItem;

            // Don't show the context menu for folders
            if (!(list == findViewById(R.id.bookmarks_list) &&
                  cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER)) {
                String keyword = null;
                int keywordCol = cursor.getColumnIndex(URLColumns.KEYWORD);
                if (keywordCol != -1)
                    keyword = cursor.getString(keywordCol);

                // Use the bookmark id for the Bookmarks tab and the history id for the Top Sites tab 
                int id = (list == findViewById(R.id.bookmarks_list)) ? cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID)) :
                                                                       cursor.getInt(cursor.getColumnIndexOrThrow(Combined.HISTORY_ID));

                mContextMenuSubject = new ContextMenuSubject(id,
                                                             cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL)),
                                                             cursor.getBlob(cursor.getColumnIndexOrThrow(URLColumns.FAVICON)),
                                                             cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE)),
                                                             keyword);
            }
        }

        if (mContextMenuSubject == null)
            return;

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.awesomebar_contextmenu, menu);
        
        if (list != findViewById(R.id.bookmarks_list)) {
            menu.findItem(R.id.remove_bookmark).setVisible(false);
            menu.findItem(R.id.edit_bookmark).setVisible(false);

            // Hide "Remove" item if there isn't a valid history ID
            if (mContextMenuSubject.id < 0)
                menu.findItem(R.id.remove_history).setVisible(false);
        } else {
            menu.findItem(R.id.remove_history).setVisible(false);
        }

        menu.setHeaderTitle(mContextMenuSubject.title);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        if (mContextMenuSubject == null)
            return false;

        final int id = mContextMenuSubject.id;
        final String url = mContextMenuSubject.url;
        final byte[] b = mContextMenuSubject.favicon;
        final String title = mContextMenuSubject.title;
        final String keyword = mContextMenuSubject.keyword;

        switch (item.getItemId()) {
            case R.id.open_new_tab: {
                if (url == null) {
                    Log.e(LOGTAG, "Can't open in new tab because URL is null");
                    break;
                }

                GeckoApp.mAppContext.loadUrl(url, AwesomeBar.Target.NEW_TAB);
                Toast.makeText(this, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
                break;
            }
            case R.id.edit_bookmark: {
                AlertDialog.Builder editPrompt = new AlertDialog.Builder(this);
                View editView = getLayoutInflater().inflate(R.layout.bookmark_edit, null);
                editPrompt.setTitle(R.string.bookmark_edit_title);
                editPrompt.setView(editView);

                final EditText nameText = ((EditText) editView.findViewById(R.id.edit_bookmark_name));
                final EditText locationText = ((EditText) editView.findViewById(R.id.edit_bookmark_location));
                final EditText keywordText = ((EditText) editView.findViewById(R.id.edit_bookmark_keyword));
                nameText.setText(title);
                locationText.setText(url);
                keywordText.setText(keyword);

                editPrompt.setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        (new GeckoAsyncTask<Void, Void, Void>() {
                            @Override
                            public Void doInBackground(Void... params) {
                                String newUrl = locationText.getText().toString().trim();
                                BrowserDB.updateBookmark(mResolver, id, newUrl, nameText.getText().toString(),
                                                         keywordText.getText().toString());
                                return null;
                            }

                            @Override
                            public void onPostExecute(Void result) {
                                Toast.makeText(AwesomeBar.this, R.string.bookmark_updated, Toast.LENGTH_SHORT).show();
                            }
                        }).execute();
                    }
                });

                editPrompt.setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
                      public void onClick(DialogInterface dialog, int whichButton) {
                          // do nothing
                      }
                });

                final AlertDialog dialog = editPrompt.create();

                // disable OK button if the URL is empty
                locationText.addTextChangedListener(new TextWatcher() {
                    private boolean mEnabled = true;

                    public void afterTextChanged(Editable s) {}

                    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        boolean enabled = (s.toString().trim().length() > 0);
                        if (mEnabled != enabled) {
                            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(enabled);
                            mEnabled = enabled;
                        }
                    }
                });

                dialog.show();
                break;
            }
            case R.id.remove_bookmark: {
                (new AsyncTask<Void, Void, Void>() {
                    private boolean mInReadingList;

                    @Override
                    public void onPreExecute() {
                        mInReadingList = mAwesomeTabs.isInReadingList();
                    }

                    @Override
                    public Void doInBackground(Void... params) {
                        BrowserDB.removeBookmark(mResolver, id);
                        return null;
                    }

                    @Override
                    public void onPostExecute(Void result) {
                        int messageId = R.string.bookmark_removed;
                        if (mInReadingList)
                            messageId = R.string.reading_list_removed;

                        Toast.makeText(AwesomeBar.this, messageId, Toast.LENGTH_SHORT).show();
                    }
                }).execute();
                break;
            }
            case R.id.remove_history: {
                (new GeckoAsyncTask<Void, Void, Void>() {
                    @Override
                    public Void doInBackground(Void... params) {
                        BrowserDB.removeHistoryEntry(mResolver, id);
                        return null;
                    }

                    @Override
                    public void onPostExecute(Void result) {
                        Toast.makeText(AwesomeBar.this, R.string.history_removed, Toast.LENGTH_SHORT).show();
                    }
                }).execute();
                break;
            }
            case R.id.add_to_launcher: {
                if (url == null) {
                    Log.e(LOGTAG, "Can't add to home screen because URL is null");
                    break;
                }

                Bitmap bitmap = null;
                if (b != null)
                    bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);

                String shortcutTitle = TextUtils.isEmpty(title) ? url.replaceAll("^([a-z]+://)?(www\\.)?", "") : title;
                GeckoAppShell.createShortcut(shortcutTitle, url, bitmap, "");
                break;
            }
            case R.id.share: {
                if (url == null) {
                    Log.e(LOGTAG, "Can't share because URL is null");
                    break;
                }

                GeckoAppShell.openUriExternal(url, "text/plain", "", "",
                                              Intent.ACTION_SEND, title);
                break;
            }
            default: {
                return super.onContextItemSelected(item);
            }
        }
        return true;
    }

    private static boolean hasCompositionString(Editable content) {
        Object[] spans = content.getSpans(0, content.length(), Object.class);
        if (spans != null) {
            for (Object span : spans) {
                if ((content.getSpanFlags(span) & Spanned.SPAN_COMPOSING) != 0) {
                    // Found composition string.
                    return true;
                }
            }
        }
        return false;
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

    private SharedPreferences getSearchPreferences() {
        return getSharedPreferences("search.prefs", MODE_PRIVATE);
    }
}
