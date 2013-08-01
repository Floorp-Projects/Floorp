/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.health.BrowserHealthRecorder;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TabWidget;
import android.widget.Toast;

import org.json.JSONObject;

import java.net.URLEncoder;

interface AutocompleteHandler {
    void onAutocomplete(String res);
}

public class AwesomeBar extends GeckoActivity
                        implements AutocompleteHandler,
                                   TextWatcher {
    private static final String LOGTAG = "GeckoAwesomeBar";

    public static final String URL_KEY = "url";
    public static final String TAB_KEY = "tab";
    public static final String CURRENT_URL_KEY = "currenturl";
    public static final String TARGET_KEY = "target";
    public static final String SEARCH_KEY = "search";
    public static final String TITLE_KEY = "title";
    public static final String USER_ENTERED_KEY = "user_entered";
    public static final String READING_LIST_KEY = "reading_list";
    public static enum Target { NEW_TAB, CURRENT_TAB, PICK_SITE };

    private String mTarget;
    private AwesomeBarTabs mAwesomeTabs;
    private CustomEditText mText;
    private ImageButton mGoButton;
    private ContextMenuSubject mContextMenuSubject;
    private boolean mDelayRestartInput;
    // The previous autocomplete result returned to us
    private String mAutoCompleteResult = "";
    // The user typed part of the autocomplete result
    private String mAutoCompletePrefix = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        LayoutInflater.from(this).setFactory(this);

        super.onCreate(savedInstanceState);

        Log.d(LOGTAG, "creating awesomebar");

        setContentView(R.layout.awesomebar);

        mGoButton = (ImageButton) findViewById(R.id.awesomebar_button);
        mText = (CustomEditText) findViewById(R.id.awesomebar_text);

        TabWidget tabWidget = (TabWidget) findViewById(android.R.id.tabs);
        tabWidget.setDividerDrawable(null);

        mAwesomeTabs = (AwesomeBarTabs) findViewById(R.id.awesomebar_tabs);
        mAwesomeTabs.setOnUrlOpenListener(new AwesomeBarTabs.OnUrlOpenListener() {
            @Override
            public void onUrlOpen(String url, String title) {
                openUrlAndFinish(url, title, false);
            }

            @Override
            public void onSearch(SearchEngine engine, String text) {
                Intent resultIntent = new Intent();
                resultIntent.putExtra(URL_KEY, text);
                resultIntent.putExtra(TARGET_KEY, mTarget);
                resultIntent.putExtra(SEARCH_KEY, engine.name);
                recordSearch(engine.identifier, "barsuggest");
                finishWithResult(resultIntent);
            }

            @Override
            public void onEditSuggestion(final String text) {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mText.setText(text);
                        mText.setSelection(mText.getText().length());
                        mText.requestFocus();
                    }
                });
            }

            @Override
            public void onSwitchToTab(final int tabId) {
                Intent resultIntent = new Intent();
                resultIntent.putExtra(TAB_KEY, Integer.toString(tabId));
                finishWithResult(resultIntent);
            }
        });

        mGoButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                openUserEnteredAndFinish(mText.getText().toString());
            }
        });

        Intent intent = getIntent();
        String currentUrl = intent.getStringExtra(CURRENT_URL_KEY);
        if (currentUrl != null) {
            mText.setText(currentUrl);
            mText.selectAll();
        }

        mTarget = intent.getStringExtra(TARGET_KEY);
        if (mTarget.equals(Target.CURRENT_TAB.name())) {
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null && tab.isPrivate()) {
                BrowserToolbarBackground mAddressBarBg = (BrowserToolbarBackground) findViewById(R.id.address_bar_bg);
                mAddressBarBg.setPrivateMode(true);

                ShapedButton mTabs = (ShapedButton) findViewById(R.id.dummy_tab);
                if (mTabs != null)
                    mTabs.setPrivateMode(true);

                mText.setPrivateMode(true);
            }
        }
        mAwesomeTabs.setTarget(mTarget);

        mText.setOnKeyPreImeListener(new CustomEditText.OnKeyPreImeListener() {
            @Override
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
                    return handleBackKey();
                }

                return false;
            }
        });

        mText.addTextChangedListener(this);

        mText.setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER || GamepadUtils.isActionKey(event)) {
                    if (event.getAction() != KeyEvent.ACTION_DOWN)
                        return true;

                    openUserEnteredAndFinish(mText.getText().toString());
                    return true;
                } else if (GamepadUtils.isBackKey(event)) {
                    return handleBackKey();
                } else {
                    return false;
                }
            }
        });

        mText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (v == null || hasFocus) {
                    return;
                }

                InputMethodManager imm = (InputMethodManager)
                    getSystemService(Context.INPUT_METHOD_SERVICE);
                try {
                    imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                } catch (NullPointerException e) {
                    Log.e(LOGTAG, "InputMethodManagerService, why are you throwing"
                                  + " a NullPointerException? See bug 782096", e);
                }
            }
        });

        boolean showReadingList = intent.getBooleanExtra(READING_LIST_KEY, false);
        if (showReadingList) {
            BookmarksTab bookmarksTab = mAwesomeTabs.getBookmarksTab();
            bookmarksTab.setShowReadingList(true);
            mAwesomeTabs.setCurrentItemByTag(bookmarksTab.getTag());
        }
    }

    /*
     * Only one factory can be set on the inflater; however, we want to use two
     * factories (GeckoViewsFactory and the FragmentActivity factory).
     * Overriding onCreateView() here allows us to dispatch view creation to
     * both factories.
     */
    @Override
    public View onCreateView(String name, Context context, AttributeSet attrs) {
        View view = GeckoViewsFactory.getInstance().onCreateView(name, context, attrs);
        if (view == null) {
            view = super.onCreateView(name, context, attrs);
        }
        return view;
    }

    private boolean handleBackKey() {
        // Let mAwesomeTabs try to handle the back press, since we may be in a
        // bookmarks sub-folder.
        if (mAwesomeTabs.onBackPressed())
            return true;

        // If mAwesomeTabs.onBackPressed() returned false, we didn't move up
        // a folder level, so just exit the activity.
        cancelAndFinish();
        return true;
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
        String contentDescription = getString(R.string.go);
        int imeAction = EditorInfo.IME_ACTION_GO;

        int actionBits = mText.getImeOptions() & EditorInfo.IME_MASK_ACTION;
        if (StringUtils.isSearchQuery(text, actionBits == EditorInfo.IME_ACTION_SEARCH)) {
            imageResource = R.drawable.ic_awesomebar_search;
            contentDescription = getString(R.string.search);
            imeAction = EditorInfo.IME_ACTION_SEARCH;
        }

        InputMethodManager imm = InputMethods.getInputMethodManager(mText.getContext());
        if (imm == null) {
            return;
        }
        boolean restartInput = false;
        if (actionBits != imeAction) {
            int optionBits = mText.getImeOptions() & ~EditorInfo.IME_MASK_ACTION;
            mText.setImeOptions(optionBits | imeAction);

            mDelayRestartInput = (imeAction == EditorInfo.IME_ACTION_GO) &&
                                 (InputMethods.shouldDelayAwesomebarUpdate(mText.getContext()));
            if (!mDelayRestartInput) {
                restartInput = true;
            }
        } else if (mDelayRestartInput) {
            // Only call delayed restartInput when actionBits == imeAction
            // so if there are two restarts in a row, the first restarts will
            // be discarded and the second restart will be properly delayed
            mDelayRestartInput = false;
            restartInput = true;
        }
        if (restartInput) {
            updateKeyboardInputType();
            imm.restartInput(mText);
            mGoButton.setImageResource(imageResource);
            mGoButton.setContentDescription(contentDescription);
        }
    }

    private void updateKeyboardInputType() {
        // If the user enters a space, then we know they are entering search terms, not a URL.
        // We can then switch to text mode so,
        // 1) the IME auto-inserts spaces between words
        // 2) the IME doesn't reset input keyboard to Latin keyboard.
        String text = mText.getText().toString();
        int currentInputType = mText.getInputType();
        int newInputType = StringUtils.isSearchQuery(text, false)
                           ? (currentInputType & ~InputType.TYPE_TEXT_VARIATION_URI) // Text mode
                           : (currentInputType | InputType.TYPE_TEXT_VARIATION_URI); // URL mode
        if (newInputType != currentInputType) {
            mText.setRawInputType(newInputType);
        }
    }

    private void cancelAndFinish() {
        setResult(Activity.RESULT_CANCELED);
        finish();
        overridePendingTransition(R.anim.awesomebar_hold_still, R.anim.awesomebar_fade_out);
    }

    private void finishWithResult(Intent intent) {
        setResult(Activity.RESULT_OK, intent);
        finish();
        overridePendingTransition(R.anim.awesomebar_hold_still, R.anim.awesomebar_fade_out);
    }

    private void openUrlAndFinish(String url) {
        openUrlAndFinish(url, null, false);
    }

    private void openUrlAndFinish(String url, String title, boolean userEntered) {
        Intent resultIntent = new Intent();
        resultIntent.putExtra(URL_KEY, url);
        if (title != null && !TextUtils.isEmpty(title))
            resultIntent.putExtra(TITLE_KEY, title);
        if (userEntered)
            resultIntent.putExtra(USER_ENTERED_KEY, userEntered);
        resultIntent.putExtra(TARGET_KEY, mTarget);
        finishWithResult(resultIntent);
    }

    /**
     * Record in Health Report that a search has occurred.
     *
     * @param identifier
     *        a search identifier, such as "partnername". Can be null.
     * @param where
     *        where the search was initialized; one of the values in
     *        {@link BrowserHealthRecorder#SEARCH_LOCATIONS}.
     */
    private static void recordSearch(String identifier, String where) {
        Log.i(LOGTAG, "Recording search: " + identifier + ", " + where);
        try {
            JSONObject message = new JSONObject();
            message.put("type", BrowserHealthRecorder.EVENT_SEARCH);
            message.put("location", where);
            message.put("identifier", identifier);
            GeckoAppShell.getEventDispatcher().dispatchEvent(message);
        } catch (Exception e) {
            Log.w(LOGTAG, "Error recording search.", e);
        }
    }

    private void openUserEnteredAndFinish(final String url) {
        final int index = url.indexOf(' ');

        // Check for a keyword if the URL looks like a search query
        if (!StringUtils.isSearchQuery(url, true)) {
            openUrlAndFinish(url, "", true);
            return;
        }
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final String keyword;
                final String keywordSearch;

                if (index == -1) {
                    keyword = url;
                    keywordSearch = "";
                } else {
                    keyword = url.substring(0, index);
                    keywordSearch = url.substring(index + 1);
                }

                final String keywordUrl = BrowserDB.getUrlForKeyword(getContentResolver(), keyword);
                final String searchUrl = (keywordUrl != null)
                                       ? keywordUrl.replace("%s", URLEncoder.encode(keywordSearch))
                                       : url;
                if (keywordUrl != null) {
                    recordSearch(null, "barkeyword");
                }
                openUrlAndFinish(searchUrl, "", true);
            }
        });
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Galaxy Note sends key events for the stylus that are outside of the
        // valid keyCode range (see bug 758427)
        if (keyCode > KeyEvent.getMaxKeyCode())
            return true;

        // This method is called only if the key event was not handled
        // by any of the views, which usually means the edit box lost focus
        if (keyCode == KeyEvent.KEYCODE_BACK ||
            keyCode == KeyEvent.KEYCODE_MENU ||
            keyCode == KeyEvent.KEYCODE_DPAD_UP ||
            keyCode == KeyEvent.KEYCODE_DPAD_DOWN ||
            keyCode == KeyEvent.KEYCODE_DPAD_LEFT ||
            keyCode == KeyEvent.KEYCODE_DPAD_RIGHT ||
            keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
            keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyDown(keyCode, event);
        } else if (keyCode == KeyEvent.KEYCODE_SEARCH) {
             mText.setText("");
             mText.requestFocus();
             return true;
        } else {
            int prevSelStart = mText.getSelectionStart();
            int prevSelEnd = mText.getSelectionEnd();

            // Manually dispatch the key event to the AwesomeBar. If selection changed as
            // a result of the key event, then give focus back to mText
            mText.dispatchKeyEvent(event);

            int curSelStart = mText.getSelectionStart();
            int curSelEnd = mText.getSelectionEnd();
            if (prevSelStart != curSelStart || prevSelEnd != curSelEnd) {
                mText.requestFocusFromTouch();
                // Restore the selection, which gets lost due to the focus switch
                mText.setSelection(curSelStart, curSelEnd);
            }
            return true;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mText != null && mText.getText() != null) {
            updateGoButton(mText.getText().toString());
            if (mDelayRestartInput) {
                // call updateGoButton again to force a restartInput call
                updateGoButton(mText.getText().toString());
            }
        }

        // Invlidate the cached value that keeps track of whether or
        // not desktop bookmarks exist
        BrowserDB.invalidateCachedState();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mAwesomeTabs.destroy();
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

    static public class ContextMenuSubject {
        public int id;
        public String url;
        public byte[] favicon;
        public String title;
        public String keyword;
        public int display;

        public ContextMenuSubject(int id, String url, byte[] favicon, String title, String keyword) {
            this(id, url, favicon, title, keyword, Combined.DISPLAY_NORMAL);
        }

        public ContextMenuSubject(int id, String url, byte[] favicon, String title, String keyword, int display) {
            this.id = id;
            this.url = url;
            this.favicon = favicon;
            this.title = title;
            this.keyword = keyword;
            this.display = display;
        }
    };

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, view, menuInfo);
        AwesomeBarTab tab = mAwesomeTabs.getAwesomeBarTabForView(view);
        mContextMenuSubject = tab.getSubject(menu, view, menuInfo);
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
        final int display = mContextMenuSubject.display;

        switch (item.getItemId()) {
            case R.id.open_private_tab:
            case R.id.open_new_tab: {
                if (url == null) {
                    Log.e(LOGTAG, "Can't open in new tab because URL is null");
                    break;
                }

                String newTabUrl = url;
                if (display == Combined.DISPLAY_READER)
                    newTabUrl = ReaderModeUtils.getAboutReaderForUrl(url, true);

                int flags = Tabs.LOADURL_NEW_TAB;
                if (item.getItemId() == R.id.open_private_tab)
                    flags |= Tabs.LOADURL_PRIVATE;

                Tabs.getInstance().loadUrl(newTabUrl, flags);
                Toast.makeText(this, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
                break;
            }
            case R.id.open_in_reader: {
                if (url == null) {
                    Log.e(LOGTAG, "Can't open in reader mode because URL is null");
                    break;
                }

                openUrlAndFinish(ReaderModeUtils.getAboutReaderForUrl(url, true));
                break;
            }
            case R.id.edit_bookmark: {
                new EditBookmarkDialog(this).show(id, title, url, keyword);
                break;
            }
            case R.id.remove_bookmark: {
                (new UiAsyncTask<Void, Void, Integer>(ThreadUtils.getBackgroundHandler()) {
                    private boolean mInReadingList;

                    @Override
                    public void onPreExecute() {
                        mInReadingList = mAwesomeTabs.isInReadingList();
                    }

                    @Override
                    public Integer doInBackground(Void... params) {
                        BrowserDB.removeBookmark(getContentResolver(), id);
                        Integer count = mInReadingList ?
                            BrowserDB.getReadingListCount(getContentResolver()) : 0;

                        return count;
                    }

                    @Override
                    public void onPostExecute(Integer aCount) {
                        int messageId = R.string.bookmark_removed;
                        if (mInReadingList) {
                            messageId = R.string.reading_list_removed;

                            GeckoEvent e = GeckoEvent.createBroadcastEvent("Reader:Remove", url);
                            GeckoAppShell.sendEventToGecko(e);

                            // Delete from Awesomebar context menu can alter reading list bookmark count
                            e = GeckoEvent.createBroadcastEvent("Reader:ListCountUpdated", Integer.toString(aCount));
                            GeckoAppShell.sendEventToGecko(e);
                        }

                        Toast.makeText(AwesomeBar.this, messageId, Toast.LENGTH_SHORT).show();
                    }
                }).execute();
                break;
            }
            case R.id.remove_history: {
                (new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
                    @Override
                    public Void doInBackground(Void... params) {
                        BrowserDB.removeHistoryEntry(getContentResolver(), id);
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
                if (b != null) {
                    bitmap = BitmapUtils.decodeByteArray(b);
                }

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

    public static String getReaderForUrl(String url) {
        // FIXME: still need to define the final way to open items from
        // reading list. For now, we're using an about:reader page.
        return "about:reader?url=" + Uri.encode(url) + "&readingList=1";
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

    // return early if we're backspacing through the string, or have no autocomplete results
    public void onAutocomplete(final String result) {
        final String text = mText.getText().toString();

        if (result == null) {
            mAutoCompleteResult = "";
            return;
        }

        if (!result.startsWith(text) || text.equals(result)) {
            return;
        }

        mAutoCompleteResult = result;
        mText.getText().append(result.substring(text.length()));
        mText.setSelection(text.length(), result.length());
    }

    @Override
    public void afterTextChanged(final Editable s) {
        final String text = s.toString();
        boolean useHandler = false;
        boolean reuseAutocomplete = false;
        if (!hasCompositionString(s) && !StringUtils.isSearchQuery(text, false)) {
            useHandler = true;

            // If you're hitting backspace (the string is getting smaller
            // or is unchanged), don't autocomplete.
            if (mAutoCompletePrefix != null && (mAutoCompletePrefix.length() >= text.length())) {
                useHandler = false;
            } else if (mAutoCompleteResult != null && mAutoCompleteResult.startsWith(text)) {
                // If this text already matches our autocomplete text, autocomplete likely
                // won't change. Just reuse the old autocomplete value.
                useHandler = false;
                reuseAutocomplete = true;
            }
        }

        // If this is the autocomplete text being set, don't run the filter.
        if (TextUtils.isEmpty(mAutoCompleteResult) || !mAutoCompleteResult.equals(text)) {
            mAwesomeTabs.filter(text, useHandler ? this : null);
            mAutoCompletePrefix = text;

            if (reuseAutocomplete) {
                onAutocomplete(mAutoCompleteResult);
            }
        }

        // If the AwesomeBar has a composition string, don't call updateGoButton().
        // That method resets IME and composition state will be broken.
        if (!hasCompositionString(s)) {
            updateGoButton(text);
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count,
                                  int after) {
        // do nothing
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before,
                              int count) {
        // do nothing
    }
}
