/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AwesomeBar.ContextMenuSubject;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoAsyncTask;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.StringUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.AdapterView;
import android.widget.FilterQueryProvider;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.List;

public class AllPagesTab extends AwesomeBarTab implements GeckoEventListener {
    public static final String LOGTAG = "ALL_PAGES";
    private static final String TAG = "allPages";

    private static final int SUGGESTION_TIMEOUT = 3000;
    private static final int SUGGESTION_MAX = 3;
    private static final int ANIMATION_DURATION = 250;

    private String mSearchTerm;
    private ArrayList<SearchEngine> mSearchEngines;
    private SuggestClient mSuggestClient;
    private boolean mSuggestionsEnabled;
    private AsyncTask<String, Void, ArrayList<String>> mSuggestTask;
    private AwesomeBarCursorAdapter mCursorAdapter = null;
    private boolean mTelemetrySent = false;
    private LinearLayout mAllPagesView;
    private boolean mAnimateSuggestions;
    private View mSuggestionsOptInPrompt;
    private Handler mHandler;
    private ListView mListView;

    private static final int MESSAGE_LOAD_FAVICONS = 1;
    private static final int MESSAGE_UPDATE_FAVICONS = 2;
    private static final int DELAY_SHOW_THUMBNAILS = 550;

    private class SearchEntryViewHolder {
        public FlowLayout suggestionView;
        public ImageView iconView;
        public LinearLayout userEnteredView;
        public TextView userEnteredTextView;
    }

    public AllPagesTab(Context context) {
        super(context);
        mSearchEngines = new ArrayList<SearchEngine>();

        registerEventListener("SearchEngines:Data");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:Get", null));

        mHandler = new AllPagesHandler();
    }

    public boolean onBackPressed() {
        return false;
    }

    public int getTitleStringId() {
        return R.string.awesomebar_all_pages_title;
    }

    public String getTag() {
        return TAG;
    }

    private ListView getListView() {
        if (mListView == null && mView != null) {
            mListView = (ListView) mView.findViewById(R.id.awesomebar_list);
        }
        return mListView;
    }

    public View getView() {
        if (mView == null) {
            mView = (LinearLayout) (LayoutInflater.from(mContext).inflate(R.layout.awesomebar_allpages_list, null));
            mView.setTag(TAG);

            ListView list = getListView();
            list.setTag(TAG);
            ((Activity)mContext).registerForContextMenu(list);
            list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                     handleItemClick(parent, view, position, id);
                }
            });

            AwesomeBarCursorAdapter adapter = getCursorAdapter();
            list.setAdapter(adapter);
            list.setOnTouchListener(mListListener);
        }

        return mView;
    }

    public void destroy() {
        AwesomeBarCursorAdapter adapter = getCursorAdapter();
        unregisterEventListener("SearchEngines:Data");
        if (adapter == null) {
            return;
        }

        Cursor cursor = adapter.getCursor();
        if (cursor != null)
            cursor.close();

        mHandler.removeMessages(MESSAGE_UPDATE_FAVICONS);
        mHandler.removeMessages(MESSAGE_LOAD_FAVICONS);
        mHandler = null;
    }

    public void filter(String searchTerm) {
        AwesomeBarCursorAdapter adapter = getCursorAdapter();
        adapter.filter(searchTerm);

        filterSuggestions(searchTerm);
        if (mSuggestionsOptInPrompt != null) {
            int visibility = TextUtils.isEmpty(searchTerm) ? View.GONE : View.VISIBLE;
            if (mSuggestionsOptInPrompt.getVisibility() != visibility) {
                mSuggestionsOptInPrompt.setVisibility(visibility);
            }
        }
    }

    /**
     * Query for suggestions, but don't show them yet.
     */
    private void primeSuggestions() {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                mSuggestClient.query(mSearchTerm);
            }
        });
    }

    private void filterSuggestions(String searchTerm) {
        // cancel previous query
        if (mSuggestTask != null) {
            mSuggestTask.cancel(true);
        }

        if (mSuggestClient != null && mSuggestionsEnabled) {
            mSuggestTask = new AsyncTask<String, Void, ArrayList<String>>() {
                protected ArrayList<String> doInBackground(String... query) {
                    return mSuggestClient.query(query[0]);
                }

                protected void onPostExecute(ArrayList<String> suggestions) {
                    setSuggestions(suggestions);
                }
            };
            mSuggestTask.execute(searchTerm);
        }
    }

    protected AwesomeBarCursorAdapter getCursorAdapter() {
        if (mCursorAdapter == null) {
            // Load the list using a custom adapter so we can create the bitmaps
            mCursorAdapter = new AwesomeBarCursorAdapter(mContext);

            mCursorAdapter.setFilterQueryProvider(new FilterQueryProvider() {
                public Cursor runQuery(CharSequence constraint) {
                    long start = SystemClock.uptimeMillis();

                    Cursor c = BrowserDB.filter(getContentResolver(), constraint, MAX_RESULTS);
                    c.getCount();

                    postLoadFavicons();

                    long end = SystemClock.uptimeMillis();
                    int time = (int)(end - start);
                    Log.i(LOGTAG, "Got cursor in " + time + "ms");

                    if (!mTelemetrySent && TextUtils.isEmpty(constraint)) {
                        Telemetry.HistogramAdd("FENNEC_AWESOMEBAR_ALLPAGES_EMPTY_TIME", time);
                        mTelemetrySent = true;
                    }
                    return c;
                }
            });
        }
        return mCursorAdapter;
    }

    private interface AwesomeBarItem {
        public void onClick();
        public ContextMenuSubject getSubject();
    }

    private class AwesomeBarCursorItem implements AwesomeBarItem {
        private Cursor mCursor;

        public AwesomeBarCursorItem(Cursor cursor) {
            mCursor = cursor;
        }

        public void onClick() {
            AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
            if (listener == null)
                return;

            String url = mCursor.getString(mCursor.getColumnIndexOrThrow(URLColumns.URL));
            String title = mCursor.getString(mCursor.getColumnIndexOrThrow(URLColumns.TITLE));
            listener.onUrlOpen(url, title);
        }

        public ContextMenuSubject getSubject() {
            // Use the history id in order to allow removing history entries
            int id = mCursor.getInt(mCursor.getColumnIndexOrThrow(Combined.HISTORY_ID));

            String keyword = null;
            int keywordCol = mCursor.getColumnIndex(URLColumns.KEYWORD);
            if (keywordCol != -1)
                keyword = mCursor.getString(keywordCol);

            final String url = mCursor.getString(mCursor.getColumnIndexOrThrow(URLColumns.URL));

            Bitmap bitmap = Favicons.getInstance().getFaviconFromMemCache(url);
            byte[] favicon = null;

            if (bitmap != null) {
                ByteArrayOutputStream stream = new ByteArrayOutputStream();
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
                favicon = stream.toByteArray();
            }

            return new ContextMenuSubject(id, url, favicon,
                                          mCursor.getString(mCursor.getColumnIndexOrThrow(URLColumns.TITLE)),
                                          keyword,
                                          mCursor.getInt(mCursor.getColumnIndexOrThrow(Combined.DISPLAY)));
        }
    }

    private class AwesomeBarSearchEngineItem implements AwesomeBarItem {
        private String mSearchEngine;

        public AwesomeBarSearchEngineItem(String searchEngine) {
            mSearchEngine = searchEngine;
        }

        public void onClick() {
            AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
            if (listener != null)
                listener.onSearch(mSearchEngine, mSearchTerm);
        }

        public ContextMenuSubject getSubject() {
            // Do not show context menu for search engine items
            return null;
        }
    }

    private class AwesomeBarCursorAdapter extends SimpleCursorAdapter {
        private static final int ROW_SEARCH = 0;
        private static final int ROW_STANDARD = 1;
        private static final int ROW_SUGGEST = 2;

        public AwesomeBarCursorAdapter(Context context) {
            super(context, -1, null, new String[] {}, new int[] {});
            mSearchTerm = "";
        }

        public void filter(String searchTerm) {
            mSearchTerm = searchTerm;
            getFilter().filter(searchTerm);
        }

        private int getSuggestEngineCount() {
            return (mSearchTerm.length() == 0 || mSuggestClient == null || !mSuggestionsEnabled) ? 0 : 1;
        }

        // Add the search engines to the number of reported results.
        @Override
        public int getCount() {
            final int resultCount = super.getCount();

            // don't show search engines or suggestions if search field is empty
            if (mSearchTerm.length() == 0)
                return resultCount;

            return resultCount + mSearchEngines.size();
        }

        // If an item is part of the cursor result set, return that entry.
        // Otherwise, return the search engine data.
        @Override
        public Object getItem(int position) {
            int engineIndex = getEngineIndex(position);

            if (engineIndex == -1) {
                // return awesomebar result
                position -= getSuggestEngineCount();
                return new AwesomeBarCursorItem((Cursor) super.getItem(position));
            }

            // return search engine
            return new AwesomeBarSearchEngineItem(mSearchEngines.get(engineIndex).name);
        }

        private int getEngineIndex(int position) {
            final int resultCount = super.getCount();
            final int suggestEngineCount = getSuggestEngineCount();

            // return suggest engine index
            if (position < suggestEngineCount)
                return position;

            // not an engine
            if (position - suggestEngineCount < resultCount)
                return -1;

            // return search engine index
            return position - resultCount;
        }

        @Override
        public int getItemViewType(int position) {
            int engine = getEngineIndex(position);
            if (engine == -1) {
                return ROW_STANDARD;
            } else if (engine == 0 && mSuggestionsEnabled) {
                // Give suggestion views their own type to prevent them from
                // sharing other recycled search engine views. Using other
                // recycled views for the suggestion row can break animations
                // (bug 815937).
                return ROW_SUGGEST;
            }
            return ROW_SEARCH;
        }

        @Override
        public int getViewTypeCount() {
            // view can be either a standard awesomebar row, a search engine
            // row, or a suggestion row
            return 3;
        }

        @Override
        public boolean isEnabled(int position) {
            // If the suggestion row only contains one item (the user-entered
            // query), allow the entire row to be clickable; clicking the row
            // has the same effect as clicking the single suggestion. If the
            // row contains multiple items, clicking the row will do nothing.
            int index = getEngineIndex(position);
            if (index != -1)
                return mSearchEngines.get(index).suggestions.isEmpty();
            return true;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            int type = getItemViewType(position);
            if (type == ROW_SEARCH || type == ROW_SUGGEST) {
                SearchEntryViewHolder viewHolder = null;

                if (convertView == null || !(convertView.getTag() instanceof SearchEntryViewHolder)) {
                    convertView = getInflater().inflate(R.layout.awesomebar_suggestion_row, getListView(), false);

                    viewHolder = new SearchEntryViewHolder();
                    viewHolder.suggestionView = (FlowLayout) convertView.findViewById(R.id.suggestion_layout);
                    viewHolder.iconView = (ImageView) convertView.findViewById(R.id.suggestion_icon);
                    viewHolder.userEnteredView = (LinearLayout) convertView.findViewById(R.id.suggestion_user_entered);
                    viewHolder.userEnteredTextView = (TextView) convertView.findViewById(R.id.suggestion_text);

                    convertView.setTag(viewHolder);
                } else {
                    viewHolder = (SearchEntryViewHolder) convertView.getTag();
                }

                bindSearchEngineView(mSearchEngines.get(getEngineIndex(position)), viewHolder);
            } else {
                AwesomeEntryViewHolder viewHolder = null;

                if (convertView == null || !(convertView.getTag() instanceof AwesomeEntryViewHolder)) {
                    convertView = getInflater().inflate(R.layout.awesomebar_row, null);

                    viewHolder = new AwesomeEntryViewHolder();
                    viewHolder.titleView = (TextView) convertView.findViewById(R.id.title);
                    viewHolder.urlView = (TextView) convertView.findViewById(R.id.url);
                    viewHolder.faviconView = (ImageView) convertView.findViewById(R.id.favicon);
                    viewHolder.bookmarkIconView = (ImageView) convertView.findViewById(R.id.bookmark_icon);

                    convertView.setTag(viewHolder);
                } else {
                    viewHolder = (AwesomeEntryViewHolder) convertView.getTag();
                }

                position -= getSuggestEngineCount();
                Cursor cursor = getCursor();
                if (!cursor.moveToPosition(position))
                    throw new IllegalStateException("Couldn't move cursor to position " + position);

                updateTitle(viewHolder.titleView, cursor);
                updateUrl(viewHolder.urlView, cursor);
                updateBookmarkIcon(viewHolder.bookmarkIconView, cursor);
                displayFavicon(viewHolder);
            }

            return convertView;
        }

        private void bindSearchEngineView(final SearchEngine engine, final SearchEntryViewHolder viewHolder) {
            // when a suggestion is clicked, do a search
            OnClickListener clickListener = new OnClickListener() {
                public void onClick(View v) {
                    AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
                    if (listener != null) {
                        String suggestion = ((TextView) v.findViewById(R.id.suggestion_text)).getText().toString();

                        // If we're not clicking the user-entered view (the
                        // first suggestion item) and the search matches a URL
                        // pattern, go to that URL. Otherwise, do a search for
                        // the term.
                        if (v != viewHolder.userEnteredView && !StringUtils.isSearchQuery(suggestion)) {
                            listener.onUrlOpen(suggestion, null);
                        } else {
                            listener.onSearch(engine.name, suggestion);
                        }
                    }
                }
            };

            // when a suggestion is long-clicked, copy the suggestion into the URL EditText
            OnLongClickListener longClickListener = new OnLongClickListener() {
                public boolean onLongClick(View v) {
                    AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
                    if (listener != null) {
                        String suggestion = ((TextView) v.findViewById(R.id.suggestion_text)).getText().toString();
                        listener.onEditSuggestion(suggestion);
                        return true;
                    }
                    return false;
                }
            };

            // set the search engine icon (e.g., Google) for the row
            FlowLayout suggestionView = viewHolder.suggestionView;
            updateFavicon(viewHolder.iconView, engine.icon);

            // user-entered search term is first suggestion
            viewHolder.userEnteredTextView.setText(mSearchTerm);
            viewHolder.userEnteredView.setOnClickListener(clickListener);
            
            // add additional suggestions given by this engine
            int recycledSuggestionCount = suggestionView.getChildCount();
            int suggestionCount = engine.suggestions.size();
            boolean showedSuggestions = false;

            for (int i = 0; i < suggestionCount; i++) {
                String suggestion = engine.suggestions.get(i);
                View suggestionItem = null;

                // reuse suggestion views from recycled view, if possible
                if (i+1 < recycledSuggestionCount) {
                    suggestionItem = suggestionView.getChildAt(i+1);
                    suggestionItem.setVisibility(View.VISIBLE);
                } else {
                    suggestionItem = getInflater().inflate(R.layout.awesomebar_suggestion_item, null);
                    ((ImageView) suggestionItem.findViewById(R.id.suggestion_magnifier)).setVisibility(View.GONE);
                    suggestionView.addView(suggestionItem);
                }
                ((TextView) suggestionItem.findViewById(R.id.suggestion_text)).setText(suggestion);

                suggestionItem.setOnClickListener(clickListener);
                suggestionItem.setOnLongClickListener(longClickListener);

                if (mAnimateSuggestions) {
                    showedSuggestions = true;
                    AlphaAnimation anim = new AlphaAnimation(0, 1);
                    anim.setDuration(ANIMATION_DURATION);
                    anim.setStartOffset(i * ANIMATION_DURATION);
                    suggestionItem.startAnimation(anim);
                }
            }
            
            // hide extra suggestions that have been recycled
            for (int i = suggestionCount + 1; i < recycledSuggestionCount; i++) {
                suggestionView.getChildAt(i).setVisibility(View.GONE);
            }

            if (showedSuggestions)
                mAnimateSuggestions = false;
        }
    };

    private class SearchEngine {
        public String name;
        public Bitmap icon;
        public ArrayList<String> suggestions;

        public SearchEngine(String name) {
            this(name, null);
        }

        public SearchEngine(String name, Bitmap icon) {
            this.name = name;
            this.icon = icon;
            this.suggestions = new ArrayList<String>();
        }
    };

    /**
     * Sets suggestions associated with the current suggest engine.
     * If there is no suggest engine, this does nothing.
     */
    private void setSuggestions(final ArrayList<String> suggestions) {
        if (mSuggestClient != null) {
            mSearchEngines.get(0).suggestions = suggestions;
            getCursorAdapter().notifyDataSetChanged();
        }
    }

    /**
     * Sets search engines to be shown for user-entered queries.
     */
    private void setSearchEngines(JSONObject data) {
        try {
            JSONObject suggest = data.getJSONObject("suggest");
            String suggestEngine = suggest.isNull("engine") ? null : suggest.getString("engine");
            String suggestTemplate = suggest.isNull("template") ? null : suggest.getString("template");
            mSuggestionsEnabled = suggest.getBoolean("enabled");
            boolean suggestionsPrompted = suggest.getBoolean("prompted");
            JSONArray engines = data.getJSONArray("searchEngines");

            mSearchEngines = new ArrayList<SearchEngine>();
            for (int i = 0; i < engines.length(); i++) {
                JSONObject engineJSON = engines.getJSONObject(i);
                String name = engineJSON.getString("name");
                String iconURI = engineJSON.getString("iconURI");
                Bitmap icon = BitmapUtils.getBitmapFromDataURI(iconURI);
                if (name.equals(suggestEngine) && suggestTemplate != null) {
                    // suggest engine should be at the front of the list
                    mSearchEngines.add(0, new SearchEngine(name, icon));

                    // The only time Tabs.getInstance().getSelectedTab() should
                    // be null is when we're restoring after a crash. We should
                    // never restore private tabs when that happens, so it
                    // should be safe to assume that null means non-private.
                    Tab tab = Tabs.getInstance().getSelectedTab();
                    if (tab == null || !tab.isPrivate())
                        mSuggestClient = new SuggestClient(GeckoApp.mAppContext, suggestTemplate, SUGGESTION_TIMEOUT, SUGGESTION_MAX);
                } else {
                    mSearchEngines.add(new SearchEngine(name, icon));
                }
            }
            mCursorAdapter.notifyDataSetChanged();

            // show suggestions opt-in if user hasn't been prompted
            if (!suggestionsPrompted && mSuggestClient != null) {
                showSuggestionsOptIn();
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error getting search engine JSON", e);
        }

        filterSuggestions(mSearchTerm);
    }

    private void showSuggestionsOptIn() {
        mSuggestionsOptInPrompt = LayoutInflater.from(mContext).inflate(R.layout.awesomebar_suggestion_prompt, (LinearLayout)getView(), false);
        GeckoTextView promptText = (GeckoTextView) mSuggestionsOptInPrompt.findViewById(R.id.suggestions_prompt_title);
        promptText.setText(getResources().getString(R.string.suggestions_prompt, mSearchEngines.get(0).name));
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null)
            promptText.setPrivateMode(tab.isPrivate());

        final View yesButton = mSuggestionsOptInPrompt.findViewById(R.id.suggestions_prompt_yes);
        final View noButton = mSuggestionsOptInPrompt.findViewById(R.id.suggestions_prompt_no);
        OnClickListener listener = new OnClickListener() {
            public void onClick(View v) {
                // Prevent the buttons from being clicked multiple times (bug 816902)
                yesButton.setOnClickListener(null);
                noButton.setOnClickListener(null);

                setSuggestionsEnabled(v == yesButton);
            }
        };
        yesButton.setOnClickListener(listener);
        noButton.setOnClickListener(listener);

        mSuggestionsOptInPrompt.setVisibility(View.GONE);
        ((LinearLayout)getView()).addView(mSuggestionsOptInPrompt, 0);
    }

    private void setSuggestionsEnabled(final boolean enabled) {
        // Clicking the yes/no buttons quickly can cause the click events be
        // queued before the listeners are removed above, so it's possible
        // setSuggestionsEnabled() can be called twice. mSuggestionsOptInPrompt
        // can be null if this happens (bug 828480).
        if (mSuggestionsOptInPrompt == null) {
            return;
        }

        // Make suggestions appear immediately after the user opts in
        primeSuggestions();

        // Pref observer in gecko will also set prompted = true
        PrefsHelper.setPref("browser.search.suggest.enabled", enabled);

        TranslateAnimation anim1 = new TranslateAnimation(0, mSuggestionsOptInPrompt.getWidth(), 0, 0);
        anim1.setDuration(ANIMATION_DURATION);
        anim1.setInterpolator(new AccelerateInterpolator());
        anim1.setFillAfter(true);
        mSuggestionsOptInPrompt.setAnimation(anim1);

        TranslateAnimation anim2 = new TranslateAnimation(0, 0, 0, -1 * mSuggestionsOptInPrompt.getHeight());
        anim2.setDuration(ANIMATION_DURATION);
        anim2.setFillAfter(true);
        anim2.setStartOffset(anim1.getDuration());
        final LinearLayout view = (LinearLayout)getView();
        anim2.setAnimationListener(new Animation.AnimationListener() {
            public void onAnimationStart(Animation a) {
                // Increase the height of the view so a gap isn't shown during animation
                view.getLayoutParams().height = view.getHeight() +
                        mSuggestionsOptInPrompt.getHeight();
                view.requestLayout();
            }
            public void onAnimationRepeat(Animation a) {}
            public void onAnimationEnd(Animation a) {
                // Removing the view immediately results in a NPE in
                // dispatchDraw(), possibly because this callback executes
                // before drawing is finished. Posting this as a Runnable fixes
                // the issue.
                view.post(new Runnable() {
                    public void run() {
                        view.removeView(mSuggestionsOptInPrompt);
                        getListView().clearAnimation();
                        mSuggestionsOptInPrompt = null;

                        if (enabled) {
                            // Reset the view height
                            view.getLayoutParams().height = LayoutParams.FILL_PARENT;

                            mSuggestionsEnabled = enabled;
                            mAnimateSuggestions = true;
                            getCursorAdapter().notifyDataSetChanged();
                            filterSuggestions(mSearchTerm);
                        }
                    }
                });
            }
        });

        mSuggestionsOptInPrompt.startAnimation(anim1);
        getListView().startAnimation(anim2);
    }

    public void handleMessage(String event, final JSONObject message) {
        if (event.equals("SearchEngines:Data")) {
            GeckoAppShell.getMainHandler().post(new Runnable() {
                public void run() {
                    setSearchEngines(message);
                }
            });
        }
    }

    public void handleItemClick(AdapterView<?> parent, View view, int position, long id) {
        ListView listview = getListView();
        if (listview == null)
            return;

        AwesomeBarItem item = (AwesomeBarItem)listview.getItemAtPosition(position);
        item.onClick();
    }

    protected void updateBookmarkIcon(ImageView bookmarkIconView, Cursor cursor) {
        int bookmarkIdIndex = cursor.getColumnIndexOrThrow(Combined.BOOKMARK_ID);
        long id = cursor.getLong(bookmarkIdIndex);

        int displayIndex = cursor.getColumnIndexOrThrow(Combined.DISPLAY);
        int display = cursor.getInt(displayIndex);

        // The bookmark id will be 0 (null in database) when the url
        // is not a bookmark.
        int visibility = (id == 0 ? View.GONE : View.VISIBLE);
        bookmarkIconView.setVisibility(visibility);

        if (display == Combined.DISPLAY_READER) {
            bookmarkIconView.setImageResource(R.drawable.ic_awesomebar_reader);
        } else {
            bookmarkIconView.setImageResource(R.drawable.ic_awesomebar_star);
        }
    }

    public ContextMenuSubject getSubject(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        ContextMenuSubject subject = null;

        if (!(menuInfo instanceof AdapterView.AdapterContextMenuInfo)) {
            Log.e(LOGTAG, "menuInfo is not AdapterContextMenuInfo");
            return subject;
        }

        ListView list = (ListView)view;
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        subject = ((AwesomeBarItem) list.getItemAtPosition(info.position)).getSubject();

        if (subject == null)
            return subject;

        MenuInflater inflater = new MenuInflater(mContext);
        inflater.inflate(R.menu.awesomebar_contextmenu, menu);
        menu.findItem(R.id.remove_bookmark).setVisible(false);
        menu.findItem(R.id.edit_bookmark).setVisible(false);
        menu.findItem(R.id.open_in_reader).setVisible(subject.display == Combined.DISPLAY_READER);

        // Hide "Remove" item if there isn't a valid history ID
        if (subject.id < 0)
            menu.findItem(R.id.remove_history).setVisible(false);

        menu.setHeaderTitle(subject.title);
        return subject;
    }

    private void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    private List<String> getUrlsWithoutFavicon() {
        List<String> urls = new ArrayList<String>();

        Cursor c = mCursorAdapter.getCursor();
        if (c == null || !c.moveToFirst())
            return urls;

        do {
            final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

            // We only want to load favicons from DB if they are not in the
            // memory cache yet.
            if (Favicons.getInstance().getFaviconFromMemCache(url) != null)
                continue;

            urls.add(url);
        } while (c.moveToNext());

        return urls;
    }

    public void storeFaviconsInMemCache(Cursor c) {
        try {
            if (c == null || !c.moveToFirst())
                return;

            do {
                final String url = c.getString(c.getColumnIndexOrThrow(Combined.URL));
                final byte[] b = c.getBlob(c.getColumnIndexOrThrow(Combined.FAVICON));
                if (b == null)
                    continue;

                Bitmap favicon = BitmapFactory.decodeByteArray(b, 0, b.length);
                if (favicon == null)
                    continue;

                Favicons.getInstance().putFaviconInMemCache(url, favicon);
            } while (c.moveToNext());
        } finally {
            if (c != null)
                c.close();
        }
    }

    private void loadFaviconsForCurrentResults() {
        final List<String> urls = getUrlsWithoutFavicon();
        if (urls.size() == 0)
            return;

        (new GeckoAsyncTask<Void, Void, Cursor>(GeckoApp.mAppContext, GeckoAppShell.getHandler()) {
            @Override
            public Cursor doInBackground(Void... params) {
                return BrowserDB.getFaviconsForUrls(getContentResolver(), urls);
            }

            @Override
            public void onPostExecute(Cursor c) {
                storeFaviconsInMemCache(c);
                postUpdateFavicons();
            }
        }).execute();
    }

    private void displayFavicon(AwesomeEntryViewHolder viewHolder) {
        final String url = viewHolder.urlView.getText().toString();
        Bitmap bitmap = Favicons.getInstance().getFaviconFromMemCache(url);
        updateFavicon(viewHolder.faviconView, bitmap);
    }

    private void updateFavicons() {
        ListView listView = getListView();
        for (int i = 0; i < listView.getChildCount(); i++) {
            final View view = listView.getChildAt(i);
            final Object tag = view.getTag();

            if (tag == null || !(tag instanceof AwesomeEntryViewHolder))
                continue;

            final AwesomeEntryViewHolder viewHolder = (AwesomeEntryViewHolder) tag;
            displayFavicon(viewHolder);
        }

        mView.invalidate();
    }

    private void postUpdateFavicons() {
        if (mHandler == null)
            return;

        Message msg = mHandler.obtainMessage(MESSAGE_UPDATE_FAVICONS,
                                             AllPagesTab.this);

        mHandler.removeMessages(MESSAGE_UPDATE_FAVICONS);
        mHandler.sendMessage(msg);
    }

    private void postLoadFavicons() {
        if (mHandler == null)
            return;

        Message msg = mHandler.obtainMessage(MESSAGE_LOAD_FAVICONS,
                                             AllPagesTab.this);

        mHandler.removeMessages(MESSAGE_LOAD_FAVICONS);
        mHandler.sendMessageDelayed(msg, 200);
    }

    private class AllPagesHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MESSAGE_LOAD_FAVICONS:
                    loadFaviconsForCurrentResults();
                    break;
                case MESSAGE_UPDATE_FAVICONS:
                    updateFavicons();
                    break;
            }
        }
    }
}
