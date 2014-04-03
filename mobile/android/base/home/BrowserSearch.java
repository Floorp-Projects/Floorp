/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.SearchEngine;
import org.mozilla.gecko.home.SearchLoader.SearchCursorLoader;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.toolbar.AutocompleteHandler;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.EnumSet;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class BrowserSearch extends HomeFragment
                           implements GeckoEventListener {
    // Logging tag name
    private static final String LOGTAG = "GeckoBrowserSearch";

    // Cursor loader ID for search query
    private static final int LOADER_ID_SEARCH = 0;

    // AsyncTask loader ID for suggestion query
    private static final int LOADER_ID_SUGGESTION = 1;

    // Timeout for the suggestion client to respond
    private static final int SUGGESTION_TIMEOUT = 3000;

    // Maximum number of results returned by the suggestion client
    private static final int SUGGESTION_MAX = 3;

    // The maximum number of rows deep in a search we'll dig
    // for an autocomplete result
    private static final int MAX_AUTOCOMPLETE_SEARCH = 20;

    // Duration for fade-in animation
    private static final int ANIMATION_DURATION = 250;

    // Holds the current search term to use in the query
    private volatile String mSearchTerm;

    // Adapter for the list of search results
    private SearchAdapter mAdapter;

    // The view shown by the fragment
    private LinearLayout mView;

    // The list showing search results
    private HomeListView mList;

    // Client that performs search suggestion queries
    private volatile SuggestClient mSuggestClient;

    // List of search engines from gecko
    private ArrayList<SearchEngine> mSearchEngines;

    // Whether search suggestions are enabled or not
    private boolean mSuggestionsEnabled;

    // Callbacks used for the search loader
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Callbacks used for the search suggestion loader
    private SuggestionLoaderCallbacks mSuggestionLoaderCallbacks;

    // Autocomplete handler used when filtering results
    private AutocompleteHandler mAutocompleteHandler;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    // On search listener
    private OnSearchListener mSearchListener;

    // On edit suggestion listener
    private OnEditSuggestionListener mEditSuggestionListener;

    // Whether the suggestions will fade in when shown
    private boolean mAnimateSuggestions;

    // Opt-in prompt view for search suggestions
    private View mSuggestionsOptInPrompt;

    public interface OnSearchListener {
        public void onSearch(SearchEngine engine, String text);
    }

    public interface OnEditSuggestionListener {
        public void onEditSuggestion(String suggestion);
    }

    public static BrowserSearch newInstance() {
        BrowserSearch browserSearch = new BrowserSearch();

        final Bundle args = new Bundle();
        args.putBoolean(HomePager.CAN_LOAD_ARG, true);
        browserSearch.setArguments(args);

        return browserSearch;
    }

    public BrowserSearch() {
        mSearchTerm = "";
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement BrowserSearch.OnUrlOpenListener");
        }

        try {
            mSearchListener = (OnSearchListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement BrowserSearch.OnSearchListener");
        }

        try {
            mEditSuggestionListener = (OnEditSuggestionListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement BrowserSearch.OnEditSuggestionListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mAutocompleteHandler = null;
        mUrlOpenListener = null;
        mSearchListener = null;
        mEditSuggestionListener = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSearchEngines = new ArrayList<SearchEngine>();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        mSearchEngines = null;
    }

    @Override
    public void onStart() {
        super.onStart();

        // Adjusting the window size when showing the keyboard results in the underlying
        // activity being painted when the keyboard is hidden (bug 933422). This can be
        // prevented by not resizing the window.
        getActivity().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
    }

    @Override
    public void onStop() {
        super.onStop();

        getActivity().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // All list views are styled to look the same with a global activity theme.
        // If the style of the list changes, inflate it from an XML.
        mView = (LinearLayout) inflater.inflate(R.layout.browser_search, container, false);
        mList = (HomeListView) mView.findViewById(R.id.home_list_view);

        return mView;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        unregisterEventListener("SearchEngines:Data");

        mList.setAdapter(null);
        mList = null;

        mView = null;
        mSuggestionsOptInPrompt = null;
        mSuggestClient = null;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mList.setTag(HomePager.LIST_TAG_BROWSER_SEARCH);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                // Perform the user-entered search if the user clicks on a search engine row.
                // This row will be disabled if suggestions (in addition to the user-entered term) are showing.
                if (view instanceof SearchEngineRow) {
                    ((SearchEngineRow) view).performUserEnteredSearch();
                    return;
                }

                // Account for the search engine rows.
                position -= getSuggestEngineCount();
                final Cursor c = mAdapter.getCursor(position);
                final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        mList.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
                // Don't do anything when the user long-clicks on a search engine row.
                if (view instanceof SearchEngineRow) {
                    return true;
                }

                // Account for the search engine rows.
                position -= getSuggestEngineCount();
                return mList.onItemLongClick(parent, view, position, id);
            }
        });

        final ListSelectionListener listener = new ListSelectionListener();
        mList.setOnItemSelectedListener(listener);
        mList.setOnFocusChangeListener(listener);

        mList.setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, android.view.KeyEvent event) {
                final View selected = mList.getSelectedView();

                if (selected instanceof SearchEngineRow) {
                    return selected.onKeyDown(keyCode, event);
                }
                return false;
            }
        });

        registerForContextMenu(mList);
        registerEventListener("SearchEngines:Data");

        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:GetVisible", null));
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize the search adapter
        mAdapter = new SearchAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Only create an instance when we need it
        mSuggestionLoaderCallbacks = null;

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public void handleMessage(String event, final JSONObject message) {
        if (event.equals("SearchEngines:Data")) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    setSearchEngines(message);
                }
            });
        }
    }

    @Override
    protected void load() {
        SearchLoader.init(getLoaderManager(), LOADER_ID_SEARCH, mCursorLoaderCallbacks, mSearchTerm);
    }

    private void handleAutocomplete(String searchTerm, Cursor c) {
        if (c == null ||
            mAutocompleteHandler == null ||
            TextUtils.isEmpty(searchTerm)) {
            return;
        }

        // Avoid searching the path if we don't have to. Currently just
        // decided by whether there is a '/' character in the string.
        final boolean searchPath = searchTerm.indexOf('/') > 0;
        final String autocompletion = findAutocompletion(searchTerm, c, searchPath);

        if (autocompletion == null || mAutocompleteHandler == null) {
            return;
        }

        // Prefetch auto-completed domain since it's a likely target
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Session:Prefetch", "http://" + autocompletion));

        mAutocompleteHandler.onAutocomplete(autocompletion);
        mAutocompleteHandler = null;
    }

    /**
     * Returns the substring of a provided URI, starting at the given offset,
     * and extending up to the end of the path segment in which the provided
     * index is found.
     *
     * For example, given
     *
     *   "www.reddit.com/r/boop/abcdef", 0, ?
     *
     * this method returns
     *
     *   ?=2:  "www.reddit.com/"
     *   ?=17: "www.reddit.com/r/boop/"
     *   ?=21: "www.reddit.com/r/boop/"
     *   ?=22: "www.reddit.com/r/boop/abcdef"
     *
     */
    private static String uriSubstringUpToMatchedPath(final String url, final int offset, final int begin) {
        final int afterEnd = url.length();

        // We want to include the trailing slash, but not other characters.
        int chop = url.indexOf('/', begin);
        if (chop != -1) {
            ++chop;
            if (chop < offset) {
                // This isn't supposed to happen. Fall back to returning the whole damn thing.
                return url;
            }
        } else {
            chop = url.indexOf('?', begin);
            if (chop == -1) {
                chop = url.indexOf('#', begin);
            }
            if (chop == -1) {
                chop = afterEnd;
            }
        }

        return url.substring(offset, chop);
    }

    private String findAutocompletion(String searchTerm, Cursor c, boolean searchPath) {
        if (!c.moveToFirst()) {
            return null;
        }

        final int searchLength = searchTerm.length();
        final int urlIndex = c.getColumnIndexOrThrow(URLColumns.URL);
        int searchCount = 0;

        do {
            final String url = c.getString(urlIndex);

            if (searchCount == 0) {
                // Prefetch the first item in the list since it's weighted the highest
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Session:Prefetch", url.toString()));
            }

            // Does the completion match against the whole URL? This will match
            // about: pages, as well as user input including "http://...".
            if (url.startsWith(searchTerm)) {
                return uriSubstringUpToMatchedPath(url, 0, searchLength);
            }

            final Uri uri = Uri.parse(url);
            final String host = uri.getHost();

            // Host may be null for about pages.
            if (host == null) {
                continue;
            }

            if (host.startsWith(searchTerm)) {
                return host + "/";
            }

            final String strippedHost = StringUtils.stripCommonSubdomains(host);
            if (strippedHost.startsWith(searchTerm)) {
                return strippedHost + "/";
            }

            ++searchCount;

            if (!searchPath) {
                continue;
            }

            // Otherwise, if we're matching paths, let's compare against the string itself.
            final int hostOffset = url.indexOf(strippedHost);
            if (hostOffset == -1) {
                // This was a URL string that parsed to a different host (normalized?).
                // Give up.
                continue;
            }

            // We already matched the non-stripped host, so now we're
            // substring-searching in the part of the URL without the common
            // subdomains.
            if (url.startsWith(searchTerm, hostOffset)) {
                // Great! Return including the rest of the path segment.
                return uriSubstringUpToMatchedPath(url, hostOffset, hostOffset + searchLength);
            }
        } while (searchCount < MAX_AUTOCOMPLETE_SEARCH && c.moveToNext());

        return null;
    }

    private void filterSuggestions() {
        if (mSuggestClient == null || !mSuggestionsEnabled) {
            return;
        }

        if (mSuggestionLoaderCallbacks == null) {
            mSuggestionLoaderCallbacks = new SuggestionLoaderCallbacks();
        }

        getLoaderManager().restartLoader(LOADER_ID_SUGGESTION, null, mSuggestionLoaderCallbacks);
    }

    private void setSuggestions(ArrayList<String> suggestions) {
        mSearchEngines.get(0).setSuggestions(suggestions);
        mAdapter.notifyDataSetChanged();
    }

    private void setSearchEngines(JSONObject data) {
        // This method is called via a Runnable posted from the Gecko thread, so
        // it's possible the fragment and/or its view has been destroyed by the
        // time we get here. If so, just abort.
        if (mView == null) {
            return;
        }

        try {
            final JSONObject suggest = data.getJSONObject("suggest");
            final String suggestEngine = suggest.optString("engine", null);
            final String suggestTemplate = suggest.optString("template", null);
            final boolean suggestionsPrompted = suggest.getBoolean("prompted");
            final JSONArray engines = data.getJSONArray("searchEngines");

            mSuggestionsEnabled = suggest.getBoolean("enabled");

            ArrayList<SearchEngine> searchEngines = new ArrayList<SearchEngine>();
            for (int i = 0; i < engines.length(); i++) {
                final JSONObject engineJSON = engines.getJSONObject(i);
                final SearchEngine engine = new SearchEngine(engineJSON);

                if (engine.name.equals(suggestEngine) && suggestTemplate != null) {
                    // Suggest engine should be at the front of the list.
                    searchEngines.add(0, engine);

                    // The only time Tabs.getInstance().getSelectedTab() should
                    // be null is when we're restoring after a crash. We should
                    // never restore private tabs when that happens, so it
                    // should be safe to assume that null means non-private.
                    Tab tab = Tabs.getInstance().getSelectedTab();
                    final boolean isPrivate = (tab != null && tab.isPrivate());

                    // Only create a new instance of SuggestClient if it hasn't been
                    // set yet. e.g. Robocop tests might set it directly before search
                    // engines are loaded.
                    if (mSuggestClient == null && !isPrivate) {
                        setSuggestClient(new SuggestClient(getActivity(), suggestTemplate,
                                    SUGGESTION_TIMEOUT, SUGGESTION_MAX));
                    }
                } else {
                    searchEngines.add(engine);
                }
            }

            mSearchEngines = searchEngines;

            if (mAdapter != null) {
                mAdapter.notifyDataSetChanged();
            }

            // Show suggestions opt-in prompt only if suggestions are not enabled yet,
            // user hasn't been prompted and we're not on a private browsing tab.
            if (!mSuggestionsEnabled && !suggestionsPrompted && mSuggestClient != null) {
                showSuggestionsOptIn();
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error getting search engine JSON", e);
        }

        filterSuggestions();
    }

    /**
     * Sets the private SuggestClient instance. Should only be called if the suggestClient is
     * null (i.e. has not yet been initialized or has been nulled). Non-private access is
     * for testing purposes only.
     */
    @RobocopTarget
    public void setSuggestClient(final SuggestClient client) {
        if (mSuggestClient != null) {
            throw new IllegalStateException("Can only set the SuggestClient if it has not " +
                    "yet been initialized!");
        }
        mSuggestClient = client;
    }

    private void showSuggestionsOptIn() {
        // Return if the ViewStub was already inflated - an inflated ViewStub is removed from the
        // View hierarchy so a second call to findViewById will return null.
        if (mSuggestionsOptInPrompt != null) {
            return;
        }

        mSuggestionsOptInPrompt = ((ViewStub) mView.findViewById(R.id.suggestions_opt_in_prompt)).inflate();

        TextView promptText = (TextView) mSuggestionsOptInPrompt.findViewById(R.id.suggestions_prompt_title);
        promptText.setText(getResources().getString(R.string.suggestions_prompt));

        final View yesButton = mSuggestionsOptInPrompt.findViewById(R.id.suggestions_prompt_yes);
        final View noButton = mSuggestionsOptInPrompt.findViewById(R.id.suggestions_prompt_no);

        final OnClickListener listener = new OnClickListener() {
            @Override
            public void onClick(View v) {
                // Prevent the buttons from being clicked multiple times (bug 816902)
                yesButton.setOnClickListener(null);
                noButton.setOnClickListener(null);

                setSuggestionsEnabled(v == yesButton);
            }
        };

        yesButton.setOnClickListener(listener);
        noButton.setOnClickListener(listener);

        // If the prompt gains focus, automatically pass focus to the
        // yes button in the prompt.
        final View prompt = mSuggestionsOptInPrompt.findViewById(R.id.prompt);
        prompt.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus) {
                    yesButton.requestFocus();
                }
            }
        });
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
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SuggestClient client = mSuggestClient;
                if (client != null) {
                    client.query(mSearchTerm);
                }
            }
        });

        // Pref observer in gecko will also set prompted = true
        PrefsHelper.setPref("browser.search.suggest.enabled", enabled);

        TranslateAnimation slideAnimation = new TranslateAnimation(0, mSuggestionsOptInPrompt.getWidth(), 0, 0);
        slideAnimation.setDuration(ANIMATION_DURATION);
        slideAnimation.setInterpolator(new AccelerateInterpolator());
        slideAnimation.setFillAfter(true);
        final View prompt = mSuggestionsOptInPrompt.findViewById(R.id.prompt);

        TranslateAnimation shrinkAnimation = new TranslateAnimation(0, 0, 0, -1 * mSuggestionsOptInPrompt.getHeight());
        shrinkAnimation.setDuration(ANIMATION_DURATION);
        shrinkAnimation.setFillAfter(true);
        shrinkAnimation.setStartOffset(slideAnimation.getDuration());
        shrinkAnimation.setAnimationListener(new Animation.AnimationListener() {
            @Override
            public void onAnimationStart(Animation a) {
                // Increase the height of the view so a gap isn't shown during animation
                mView.getLayoutParams().height = mView.getHeight() +
                        mSuggestionsOptInPrompt.getHeight();
                mView.requestLayout();
            }

            @Override
            public void onAnimationRepeat(Animation a) {}

            @Override
            public void onAnimationEnd(Animation a) {
                // Removing the view immediately results in a NPE in
                // dispatchDraw(), possibly because this callback executes
                // before drawing is finished. Posting this as a Runnable fixes
                // the issue.
                mView.post(new Runnable() {
                    @Override
                    public void run() {
                        mView.removeView(mSuggestionsOptInPrompt);
                        mList.clearAnimation();
                        mSuggestionsOptInPrompt = null;

                        if (enabled) {
                            // Reset the view height
                            mView.getLayoutParams().height = LayoutParams.MATCH_PARENT;

                            mSuggestionsEnabled = enabled;
                            mAnimateSuggestions = true;
                            mAdapter.notifyDataSetChanged();
                            filterSuggestions();
                        }
                    }
                });
            }
        });

        prompt.startAnimation(slideAnimation);
        mSuggestionsOptInPrompt.startAnimation(shrinkAnimation);
        mList.startAnimation(shrinkAnimation);
    }

    private int getSuggestEngineCount() {
        return (TextUtils.isEmpty(mSearchTerm) || mSuggestClient == null || !mSuggestionsEnabled) ? 0 : 1;
    }

    private void registerEventListener(String eventName) {
        GeckoAppShell.registerEventListener(eventName, this);
    }

    private void unregisterEventListener(String eventName) {
        GeckoAppShell.unregisterEventListener(eventName, this);
    }

    private void restartSearchLoader() {
        SearchLoader.restart(getLoaderManager(), LOADER_ID_SEARCH, mCursorLoaderCallbacks, mSearchTerm);
    }

    private void initSearchLoader() {
        SearchLoader.init(getLoaderManager(), LOADER_ID_SEARCH, mCursorLoaderCallbacks, mSearchTerm);
    }

    public void filter(String searchTerm, AutocompleteHandler handler) {
        if (TextUtils.isEmpty(searchTerm)) {
            return;
        }

        final boolean isNewFilter = !TextUtils.equals(mSearchTerm, searchTerm);

        mSearchTerm = searchTerm;
        mAutocompleteHandler = handler;

        if (isVisible()) {
            if (isNewFilter) {
                // The adapter depends on the search term to determine its number
                // of items. Make it we notify the view about it.
                mAdapter.notifyDataSetChanged();

                // Restart loaders with the new search term
                restartSearchLoader();
                filterSuggestions();
            } else {
                // The search term hasn't changed, simply reuse any existing
                // loader for the current search term. This will ensure autocompletion
                // is consistently triggered (see bug 933739).
                initSearchLoader();
            }
        }
    }

    private static class SuggestionAsyncLoader extends AsyncTaskLoader<ArrayList<String>> {
        private final SuggestClient mSuggestClient;
        private final String mSearchTerm;
        private ArrayList<String> mSuggestions;

        public SuggestionAsyncLoader(Context context, SuggestClient suggestClient, String searchTerm) {
            super(context);
            mSuggestClient = suggestClient;
            mSearchTerm = searchTerm;
            mSuggestions = null;
        }

        @Override
        public ArrayList<String> loadInBackground() {
            return mSuggestClient.query(mSearchTerm);
        }

        @Override
        public void deliverResult(ArrayList<String> suggestions) {
            mSuggestions = suggestions;

            if (isStarted()) {
                super.deliverResult(mSuggestions);
            }
        }

        @Override
        protected void onStartLoading() {
            if (mSuggestions != null) {
                deliverResult(mSuggestions);
            }

            if (takeContentChanged() || mSuggestions == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        protected void onReset() {
            super.onReset();

            onStopLoading();
            mSuggestions = null;
        }
    }

    private class SearchAdapter extends MultiTypeCursorAdapter {
        private static final int ROW_SEARCH = 0;
        private static final int ROW_STANDARD = 1;
        private static final int ROW_SUGGEST = 2;

        public SearchAdapter(Context context) {
            super(context, null, new int[] { ROW_STANDARD,
                                             ROW_SEARCH,
                                             ROW_SUGGEST },
                                 new int[] { R.layout.home_item_row,
                                             R.layout.home_search_item_row,
                                             R.layout.home_search_item_row });
        }

        @Override
        public int getItemViewType(int position) {
            final int engine = getEngineIndex(position);

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
        public boolean isEnabled(int position) {
            // If we're using a gamepad or keyboard, allow the row to be
            // focused so it can pass the focus to its child suggestion views.
            if (!mList.isInTouchMode()) {
                return true;
            }

            // If the suggestion row only contains one item (the user-entered
            // query), allow the entire row to be clickable; clicking the row
            // has the same effect as clicking the single suggestion. If the
            // row contains multiple items, clicking the row will do nothing.
            final int index = getEngineIndex(position);
            if (index != -1) {
                return !mSearchEngines.get(index).hasSuggestions();
            }

            return true;
        }

        // Add the search engines to the number of reported results.
        @Override
        public int getCount() {
            final int resultCount = super.getCount();

            // Don't show search engines or suggestions if search field is empty
            if (TextUtils.isEmpty(mSearchTerm)) {
                return resultCount;
            }

            return resultCount + mSearchEngines.size();
        }

        @Override
        public void bindView(View view, Context context, int position) {
            final int type = getItemViewType(position);

            if (type == ROW_SEARCH || type == ROW_SUGGEST) {
                final SearchEngineRow row = (SearchEngineRow) view;
                row.setOnUrlOpenListener(mUrlOpenListener);
                row.setOnSearchListener(mSearchListener);
                row.setOnEditSuggestionListener(mEditSuggestionListener);
                row.setSearchTerm(mSearchTerm);

                final SearchEngine engine = mSearchEngines.get(getEngineIndex(position));
                final boolean animate = (mAnimateSuggestions && engine.hasSuggestions());
                row.updateFromSearchEngine(engine, animate);
                if (animate) {
                    // Only animate suggestions the first time they are shown
                    mAnimateSuggestions = false;
                }
            } else {
                // Account for the search engines
                position -= getSuggestEngineCount();

                final Cursor c = getCursor(position);
                final TwoLinePageRow row = (TwoLinePageRow) view;
                row.updateFromCursor(c);
            }
        }

        private int getEngineIndex(int position) {
            final int resultCount = super.getCount();
            final int suggestEngineCount = getSuggestEngineCount();

            // Return suggest engine index
            if (position < suggestEngineCount) {
                return position;
            }

            // Not an engine
            if (position - suggestEngineCount < resultCount) {
                return -1;
            }

            // Return search engine index
            return position - resultCount;
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return SearchLoader.createInstance(getActivity(), args);
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            mAdapter.swapCursor(c);

            // We should handle autocompletion based on the search term
            // associated with the loader that has just provided
            // the results.
            SearchCursorLoader searchLoader = (SearchCursorLoader) loader;
            handleAutocomplete(searchLoader.getSearchTerm(), c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mAdapter.swapCursor(null);
        }
    }

    private class SuggestionLoaderCallbacks implements LoaderCallbacks<ArrayList<String>> {
        @Override
        public Loader<ArrayList<String>> onCreateLoader(int id, Bundle args) {
            // mSuggestClient is set to null in onDestroyView(), so using it
            // safely here relies on the fact that onCreateLoader() is called
            // synchronously in restartLoader().
            return new SuggestionAsyncLoader(getActivity(), mSuggestClient, mSearchTerm);
        }

        @Override
        public void onLoadFinished(Loader<ArrayList<String>> loader, ArrayList<String> suggestions) {
            setSuggestions(suggestions);
        }

        @Override
        public void onLoaderReset(Loader<ArrayList<String>> loader) {
            setSuggestions(new ArrayList<String>());
        }
    }

    private static class ListSelectionListener implements View.OnFocusChangeListener,
                                                          AdapterView.OnItemSelectedListener {
        private SearchEngineRow mSelectedEngineRow;

        @Override
        public void onFocusChange(View v, boolean hasFocus) {
            if (hasFocus) {
                View selectedRow = ((ListView) v).getSelectedView();
                if (selectedRow != null) {
                    selectRow(selectedRow);
                }
            } else {
                deselectRow();
            }
        }

        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
            deselectRow();
            selectRow(view);
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            deselectRow();
        }

        private void selectRow(View row) {
            if (row instanceof SearchEngineRow) {
                mSelectedEngineRow = (SearchEngineRow) row;
                mSelectedEngineRow.onSelected();
            }
        }

        private void deselectRow() {
            if (mSelectedEngineRow != null) {
                mSelectedEngineRow.onDeselected();
                mSelectedEngineRow = null;
            }
        }
    }

    /**
     * HomeSearchListView is a list view for displaying search engine results on the awesome screen.
     */
    public static class HomeSearchListView extends HomeListView {
        public HomeSearchListView(Context context, AttributeSet attrs) {
            this(context, attrs, R.attr.homeListViewStyle);
        }

        public HomeSearchListView(Context context, AttributeSet attrs, int defStyle) {
            super(context, attrs, defStyle);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                // Dismiss the soft keyboard.
                requestFocus();
            }

            return super.onTouchEvent(event);
        }
    }
}
