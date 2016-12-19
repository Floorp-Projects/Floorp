/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;

import android.content.SharedPreferences;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SuggestClient;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.SearchLoader.SearchCursorLoader;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.toolbar.AutocompleteHandler;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.view.ContextMenu.ContextMenuInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowManager.LayoutParams;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class BrowserSearch extends HomeFragment
                           implements BundleEventListener,
                                      SearchEngineBar.OnSearchBarClickListener {

    @RobocopTarget
    public interface SuggestClientFactory {
        public SuggestClient getSuggestClient(Context context, String template, int timeout, int max);
    }

    @RobocopTarget
    public static class DefaultSuggestClientFactory implements SuggestClientFactory {
        @Override
        public SuggestClient getSuggestClient(Context context, String template, int timeout, int max) {
            return new SuggestClient(context, template, timeout, max, true);
        }
    }

    /**
     * Set this to mock the suggestion mechanism. Public for access from tests.
     */
    @RobocopTarget
    public static volatile SuggestClientFactory sSuggestClientFactory = new DefaultSuggestClientFactory();

    // Logging tag name
    private static final String LOGTAG = "GeckoBrowserSearch";

    // Cursor loader ID for search query
    private static final int LOADER_ID_SEARCH = 0;

    // AsyncTask loader ID for suggestion query
    private static final int LOADER_ID_SUGGESTION = 1;
    private static final int LOADER_ID_SAVED_SUGGESTION = 2;

    // Timeout for the suggestion client to respond
    private static final int SUGGESTION_TIMEOUT = 3000;

    // Maximum number of suggestions from the search engine's suggestion client. This impacts network traffic and device
    // data consumption whereas R.integer.max_saved_suggestions controls how many suggestion to show in the UI.
    private static final int NETWORK_SUGGESTION_MAX = 3;

    // The maximum number of rows deep in a search we'll dig
    // for an autocomplete result
    private static final int MAX_AUTOCOMPLETE_SEARCH = 20;

    // Length of https:// + 1 required to make autocomplete
    // fill in the domain, for both http:// and https://
    private static final int HTTPS_PREFIX_LENGTH = 9;

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

    // The bar on the bottom of the screen displaying search engine options.
    private SearchEngineBar mSearchEngineBar;

    // Client that performs search suggestion queries.
    // Public for testing.
    @RobocopTarget
    public volatile SuggestClient mSuggestClient;

    // List of search engines from Gecko.
    // Do not mutate this list.
    // Access to this member must only occur from the UI thread.
    private List<SearchEngine> mSearchEngines;

    // Search history suggestions
    private ArrayList<String> mSearchHistorySuggestions;

    // Track the locale that was last in use when we filled mSearchEngines.
    // Access to this member must only occur from the UI thread.
    private Locale mLastLocale;

    // Whether search suggestions are enabled or not
    private boolean mSuggestionsEnabled;

    // Whether history suggestions are enabled or not
    private boolean mSavedSearchesEnabled;

    // Callbacks used for the search loader
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Callbacks used for the search suggestion loader
    private SearchEngineSuggestionLoaderCallbacks mSearchEngineSuggestionLoaderCallbacks;
    private SearchHistorySuggestionLoaderCallbacks mSearchHistorySuggestionLoaderCallback;

    // Autocomplete handler used when filtering results
    private AutocompleteHandler mAutocompleteHandler;

    // On search listener
    private OnSearchListener mSearchListener;

    // On edit suggestion listener
    private OnEditSuggestionListener mEditSuggestionListener;

    // Whether the suggestions will fade in when shown
    private boolean mAnimateSuggestions;

    // Opt-in prompt view for search suggestions
    private View mSuggestionsOptInPrompt;

    public interface OnSearchListener {
        void onSearch(SearchEngine engine, String text, TelemetryContract.Method method);
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
        mSearchListener = null;
        mEditSuggestionListener = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSearchEngines = new ArrayList<SearchEngine>();
        mSearchHistorySuggestions = new ArrayList<>();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        mSearchEngines = null;
    }

    @Override
    public void onHiddenChanged(boolean hidden) {
        if (!hidden) {
            final Tab tab = Tabs.getInstance().getSelectedTab();
            final boolean isPrivate = (tab != null && tab.isPrivate());

            // Removes Search Suggestions Loader if in private browsing mode
            // Loader may have been inserted when browsing in normal tab
            if (isPrivate) {
                getLoaderManager().destroyLoader(LOADER_ID_SUGGESTION);
            }

            GeckoAppShell.notifyObservers("SearchEngines:GetVisible", null);
        }
        super.onHiddenChanged(hidden);
    }

    @Override
    public void onResume() {
        super.onResume();

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(getContext());
        mSavedSearchesEnabled = prefs.getBoolean(GeckoPreferences.PREFS_HISTORY_SAVED_SEARCH, true);

        // Fetch engines if we need to.
        if (mSearchEngines.isEmpty() || !Locale.getDefault().equals(mLastLocale)) {
            GeckoAppShell.notifyObservers("SearchEngines:GetVisible", null);
        } else {
            updateSearchEngineBar();
        }

        Telemetry.startUISession(TelemetryContract.Session.FRECENCY);
    }

    @Override
    public void onPause() {
        super.onPause();

        Telemetry.stopUISession(TelemetryContract.Session.FRECENCY);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // All list views are styled to look the same with a global activity theme.
        // If the style of the list changes, inflate it from an XML.
        mView = (LinearLayout) inflater.inflate(R.layout.browser_search, container, false);
        mList = (HomeListView) mView.findViewById(R.id.home_list_view);
        mSearchEngineBar = (SearchEngineBar) mView.findViewById(R.id.search_engine_bar);

        return mView;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        EventDispatcher.getInstance().unregisterUiThreadListener(this,
            "SearchEngines:Data");

        mSearchEngineBar.setAdapter(null);
        mSearchEngineBar = null;

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
                position -= getPrimaryEngineCount();
                final Cursor c = mAdapter.getCursor(position);
                final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, "frecency");

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
                position -= getPrimaryEngineCount();
                return mList.onItemLongClick(parent, view, position, id);
            }
        });

        final ListSelectionListener listener = new ListSelectionListener();
        mList.setOnItemSelectedListener(listener);
        mList.setOnFocusChangeListener(listener);

        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.Factory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.TITLE));

                int bookmarkId = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Combined.BOOKMARK_ID));
                info.bookmarkId = bookmarkId;

                int historyId = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID));
                info.historyId = historyId;

                boolean isBookmark = bookmarkId != -1;
                boolean isHistory = historyId != -1;

                if (isBookmark && isHistory) {
                    info.itemType = HomeContextMenuInfo.RemoveItemType.COMBINED;
                } else if (isBookmark) {
                    info.itemType = HomeContextMenuInfo.RemoveItemType.BOOKMARKS;
                } else if (isHistory) {
                    info.itemType = HomeContextMenuInfo.RemoveItemType.HISTORY;
                }

                return info;
            }
        });

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
        EventDispatcher.getInstance().registerUiThreadListener(this,
            "SearchEngines:Data");

        mSearchEngineBar.setOnSearchBarClickListener(this);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        if (!(menuInfo instanceof HomeContextMenuInfo)) {
            return;
        }

        HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;

        MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.browsersearch_contextmenu, menu);

        menu.setHeaderTitle(info.getDisplayTitle());
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        ContextMenuInfo menuInfo = item.getMenuInfo();
        if (!(menuInfo instanceof HomeContextMenuInfo)) {
            return false;
        }

        final HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;
        final Context context = getActivity();

        final int itemId = item.getItemId();

        if (itemId == R.id.browsersearch_remove) {
            // Position for Top Sites grid items, but will always be -1 since this is only for BrowserSearch result
            final int position = -1;

            new RemoveItemByUrlTask(context, info.url, info.itemType, position).execute();
            return true;
        }

        return false;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Initialize the search adapter
        mAdapter = new SearchAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Only create an instance when we need it
        mSearchEngineSuggestionLoaderCallbacks = null;
        mSearchHistorySuggestionLoaderCallback = null;

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (event.equals("SearchEngines:Data")) {
            setSearchEngines(message);
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
        GeckoAppShell.notifyObservers("Session:Prefetch", "http://" + autocompletion);

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

    LinkedHashSet<String> domains = null;
    private LinkedHashSet<String> getDomains() {
        if (domains == null) {
            domains = new LinkedHashSet<String>(500);
            BufferedReader buf = null;
            try {
                buf = new BufferedReader(new InputStreamReader(getResources().openRawResource(R.raw.topdomains)));
                String res = null;

                do {
                    res = buf.readLine();
                    if (res != null) {
                        domains.add(res);
                    }
                } while (res != null);
            } catch (IOException e) {
                Log.e(LOGTAG, "Error reading domains", e);
            } finally {
                if (buf != null) {
                    try {
                        buf.close();
                    } catch (IOException e) { }
                }
            }
        }
        return domains;
    }

    private String searchDomains(String search) {
        for (String domain : getDomains()) {
            if (domain.startsWith(search)) {
                return domain;
            }
        }
        return null;
    }

    private String findAutocompletion(String searchTerm, Cursor c, boolean searchPath) {
        if (!c.moveToFirst()) {
            // No cursor probably means no history, so let's try the fallback list.
            return searchDomains(searchTerm);
        }

        final int searchLength = searchTerm.length();
        final int urlIndex = c.getColumnIndexOrThrow(History.URL);
        int searchCount = 0;

        do {
            final String url = c.getString(urlIndex);

            if (searchCount == 0) {
                // Prefetch the first item in the list since it's weighted the highest
                GeckoAppShell.notifyObservers("Session:Prefetch", url);
            }

            // Does the completion match against the whole URL? This will match
            // about: pages, as well as user input including "http://...".
            if (url.startsWith(searchTerm)) {
                return uriSubstringUpToMatchedPath(url, 0,
                        (searchLength > HTTPS_PREFIX_LENGTH) ? searchLength : HTTPS_PREFIX_LENGTH);
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

        // If we can't find an autocompletion domain from history, let's try using the fallback list.
        return searchDomains(searchTerm);
    }

    public void resetScrollState() {
        mSearchEngineBar.scrollToPosition(0);
    }

    private void filterSuggestions() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        final boolean isPrivate = (tab != null && tab.isPrivate());

        // mSuggestClient may be null if we haven't received our search engine list yet - hence
        // we need to exit here in that case.
        if (isPrivate || mSuggestClient == null || (!mSuggestionsEnabled && !mSavedSearchesEnabled)) {
            mSearchHistorySuggestions.clear();
            return;
        }

        // Suggestions from search engine
        if (mSearchEngineSuggestionLoaderCallbacks == null) {
            mSearchEngineSuggestionLoaderCallbacks = new SearchEngineSuggestionLoaderCallbacks();
        }
        getLoaderManager().restartLoader(LOADER_ID_SUGGESTION, null, mSearchEngineSuggestionLoaderCallbacks);

        // Saved suggestions
        if (mSearchHistorySuggestionLoaderCallback == null) {
            mSearchHistorySuggestionLoaderCallback = new SearchHistorySuggestionLoaderCallbacks();
        }
        getLoaderManager().restartLoader(LOADER_ID_SAVED_SUGGESTION, null, mSearchHistorySuggestionLoaderCallback);
    }

    private void setSuggestions(ArrayList<String> suggestions) {
        ThreadUtils.assertOnUiThread();

        // mSearchEngines may be null if the setSuggestions calls after onDestroy (bug 1310621).
        // So drop the suggestions if search engines are not available
        if (mSearchEngines != null && !mSearchEngines.isEmpty()) {
            mSearchEngines.get(0).setSuggestions(suggestions);
            mAdapter.notifyDataSetChanged();
        }

    }

    private void setSavedSuggestions(ArrayList<String> savedSuggestions) {
        ThreadUtils.assertOnUiThread();

        mSearchHistorySuggestions = savedSuggestions;
        mAdapter.notifyDataSetChanged();
    }

    private boolean shouldUpdateSearchEngine(ArrayList<SearchEngine> searchEngines) {
        if (searchEngines.size() != mSearchEngines.size()) {
            return true;
        }

        int size = searchEngines.size();

        for (int i = 0; i < size; i++) {
            if (!mSearchEngines.get(i).name.equals(searchEngines.get(i).name)) {
                return true;
            }
        }

        return false;
    }

    private void setSearchEngines(final GeckoBundle data) {
        ThreadUtils.assertOnUiThread();

        // This method is called via a Runnable posted from the Gecko thread, so
        // it's possible the fragment and/or its view has been destroyed by the
        // time we get here. If so, just abort.
        if (mView == null) {
            return;
        }

        final GeckoBundle suggest = data.getBundle("suggest");
        final String suggestEngine = suggest.getString("engine", null);
        final String suggestTemplate = suggest.getString("template", null);
        final boolean suggestionsPrompted = suggest.getBoolean("prompted");
        final GeckoBundle[] engines = data.getBundleArray("searchEngines");

        mSuggestionsEnabled = suggest.getBoolean("enabled");

        ArrayList<SearchEngine> searchEngines = new ArrayList<SearchEngine>();
        for (int i = 0; i < engines.length; i++) {
            final GeckoBundle engineBundle = engines[i];
            final SearchEngine engine = new SearchEngine((Context) getActivity(), engineBundle);

            if (engine.name.equals(suggestEngine) && suggestTemplate != null) {
                // Suggest engine should be at the front of the list.
                // We're baking in an assumption here that the suggest engine
                // is also the default engine.
                searchEngines.add(0, engine);

                ensureSuggestClientIsSet(suggestTemplate);
            } else {
                searchEngines.add(engine);
            }
        }

        // checking if the new searchEngine is different from mSearchEngine, will have to
        // re-layout if yes
        boolean change = shouldUpdateSearchEngine(searchEngines);

        if (mAdapter != null && change) {
            mSearchEngines = Collections.unmodifiableList(searchEngines);
            mLastLocale = Locale.getDefault();
            updateSearchEngineBar();

            mAdapter.notifyDataSetChanged();
        }

        final Tab tab = Tabs.getInstance().getSelectedTab();
        final boolean isPrivate = (tab != null && tab.isPrivate());

        // Show suggestions opt-in prompt only if suggestions are not enabled yet,
        // user hasn't been prompted and we're not on a private browsing tab.
        // The prompt might have been inflated already when this view was previously called.
        // Remove the opt-in prompt if it has been inflated in the view and dealt with by the user,
        // or if we're on a private browsing tab
        if (!mSuggestionsEnabled && !suggestionsPrompted && !isPrivate) {
            showSuggestionsOptIn();
        } else {
            removeSuggestionsOptIn();
        }

        filterSuggestions();
    }

    private void updateSearchEngineBar() {
        final int primaryEngineCount = getPrimaryEngineCount();

        if (primaryEngineCount < mSearchEngines.size()) {
            mSearchEngineBar.setSearchEngines(
                    mSearchEngines.subList(primaryEngineCount, mSearchEngines.size())
            );
            mSearchEngineBar.setVisibility(View.VISIBLE);
        } else {
            mSearchEngineBar.setVisibility(View.GONE);
        }
    }

    @Override
    public void onSearchBarClickListener(final SearchEngine searchEngine) {
        final TelemetryContract.Method method = TelemetryContract.Method.LIST_ITEM;
        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, method, "searchenginebar");
        mSearchListener.onSearch(searchEngine, mSearchTerm, method);
    }

    private void ensureSuggestClientIsSet(final String suggestTemplate) {
        // Don't update the suggestClient if we already have a client with the correct template
        if (mSuggestClient != null && suggestTemplate.equals(mSuggestClient.getSuggestTemplate())) {
            return;
        }

        mSuggestClient = sSuggestClientFactory.getSuggestClient(getActivity(), suggestTemplate, SUGGESTION_TIMEOUT, NETWORK_SUGGESTION_MAX);
    }

    private void showSuggestionsOptIn() {
        // Only make the ViewStub visible again if it has already previously been shown.
        // (An inflated ViewStub is removed from the View hierarchy so a second call to findViewById will return null,
        // which also further necessitates handling this separately.)
        if (mSuggestionsOptInPrompt != null) {
            mSuggestionsOptInPrompt.setVisibility(View.VISIBLE);
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

    private void removeSuggestionsOptIn() {
        if (mSuggestionsOptInPrompt == null) {
            return;
        }

        mSuggestionsOptInPrompt.setVisibility(View.GONE);
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

        PrefsHelper.setPref("browser.search.suggest.prompted", true);
        PrefsHelper.setPref("browser.search.suggest.enabled", enabled);

        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, (enabled ? "suggestions_optin_yes" : "suggestions_optin_no"));

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

                        // Reset the view height
                        mView.getLayoutParams().height = LayoutParams.MATCH_PARENT;

                        // Show search suggestions and update them
                        if (enabled) {
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

    private int getPrimaryEngineCount() {
        return mSearchEngines.size() > 0 ? 1 : 0;
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

        if (mAdapter != null) {
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

    abstract private static class SuggestionAsyncLoader extends AsyncTaskLoader<ArrayList<String>> {
        protected final String mSearchTerm;
        private ArrayList<String> mSuggestions;

        public SuggestionAsyncLoader(Context context, String searchTerm) {
            super(context);
            mSearchTerm = searchTerm;
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

    private static class SearchEngineSuggestionAsyncLoader extends SuggestionAsyncLoader {
        private final SuggestClient mSuggestClient;

        public SearchEngineSuggestionAsyncLoader(Context context, SuggestClient suggestClient, String searchTerm) {
            super(context, searchTerm);
            mSuggestClient = suggestClient;
        }

        @Override
        public ArrayList<String> loadInBackground() {
            return mSuggestClient.query(mSearchTerm);
        }
    }

    private static class SearchHistorySuggestionAsyncLoader extends SuggestionAsyncLoader {
        public SearchHistorySuggestionAsyncLoader(Context context, String searchTerm) {
            super(context, searchTerm);
        }

        @Override
        public ArrayList<String> loadInBackground() {
            final ContentResolver cr = getContext().getContentResolver();

            String[] columns = new String[] { BrowserContract.SearchHistory.QUERY };
            String actualQuery = BrowserContract.SearchHistory.QUERY + " LIKE ?";
            String[] queryArgs = new String[] { '%' + mSearchTerm + '%' };

            // For deduplication, the worst case is that all the first NETWORK_SUGGESTION_MAX history suggestions are duplicates
            // of search engine suggestions, and the there is a duplicate for the search term itself. A duplicate of the
            // search term  can occur if the user has previously searched for the same thing.
            final int maxSavedSuggestions = NETWORK_SUGGESTION_MAX + 1 + getContext().getResources().getInteger(R.integer.max_saved_suggestions);

            final String sortOrderAndLimit = BrowserContract.SearchHistory.DATE + " DESC LIMIT " + maxSavedSuggestions;
            final Cursor result =  cr.query(BrowserContract.SearchHistory.CONTENT_URI, columns, actualQuery, queryArgs, sortOrderAndLimit);

            if (result == null) {
                return new ArrayList<>();
            }

            final ArrayList<String> savedSuggestions = new ArrayList<>();
            try {
                if (result.moveToFirst()) {
                    final int searchColumn = result.getColumnIndexOrThrow(BrowserContract.SearchHistory.QUERY);
                    do {
                        final String savedSearch = result.getString(searchColumn);
                        savedSuggestions.add(savedSearch);
                    } while (result.moveToNext());
                }
            } finally {
                result.close();
            }

            return savedSuggestions;
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
            if (position < getPrimaryEngineCount()) {
                if (mSuggestionsEnabled && mSearchEngines.get(position).hasSuggestions()) {
                    // Give suggestion views their own type to prevent them from
                    // sharing other recycled search result views. Using other
                    // recycled views for the suggestion row can break animations
                    // (bug 815937).

                    return ROW_SUGGEST;
                } else {
                    return ROW_SEARCH;
                }
            }

            return ROW_STANDARD;
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

            if (position < getPrimaryEngineCount()) {
                return !mSearchEngines.get(position).hasSuggestions();
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

            return resultCount + getPrimaryEngineCount();
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

                final SearchEngine engine = mSearchEngines.get(position);
                final boolean haveSuggestions = (engine.hasSuggestions() || !mSearchHistorySuggestions.isEmpty());
                final boolean animate = (mAnimateSuggestions && haveSuggestions);
                row.updateSuggestions(mSuggestionsEnabled, engine, mSearchHistorySuggestions, animate);
                if (animate) {
                    // Only animate suggestions the first time they are shown
                    mAnimateSuggestions = false;
                }
            } else {
                // Account for the search engines
                position -= getPrimaryEngineCount();

                final Cursor c = getCursor(position);
                final TwoLinePageRow row = (TwoLinePageRow) view;
                row.updateFromCursor(c);
            }
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return SearchLoader.createInstance(getActivity(), args);
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            if (mAdapter != null) {
                mAdapter.swapCursor(c);

                // We should handle autocompletion based on the search term
                // associated with the loader that has just provided
                // the results.
                SearchCursorLoader searchLoader = (SearchCursorLoader) loader;
                handleAutocomplete(searchLoader.getSearchTerm(), c);
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (mAdapter != null) {
                mAdapter.swapCursor(null);
            }
        }
    }

    private class SearchEngineSuggestionLoaderCallbacks implements LoaderCallbacks<ArrayList<String>> {
        @Override
        public Loader<ArrayList<String>> onCreateLoader(int id, Bundle args) {
            // mSuggestClient is set to null in onDestroyView(), so using it
            // safely here relies on the fact that onCreateLoader() is called
            // synchronously in restartLoader().
            return new SearchEngineSuggestionAsyncLoader(getActivity(), mSuggestClient, mSearchTerm);
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

    private class SearchHistorySuggestionLoaderCallbacks implements LoaderCallbacks<ArrayList<String>> {
        @Override
        public Loader<ArrayList<String>> onCreateLoader(int id, Bundle args) {
            // mSuggestClient is set to null in onDestroyView(), so using it
            // safely here relies on the fact that onCreateLoader() is called
            // synchronously in restartLoader().
            return new SearchHistorySuggestionAsyncLoader(getActivity(), mSearchTerm);
        }

        @Override
        public void onLoadFinished(Loader<ArrayList<String>> loader, ArrayList<String> suggestions) {
            setSavedSuggestions(suggestions);
        }

        @Override
        public void onLoaderReset(Loader<ArrayList<String>> loader) {
            setSavedSuggestions(new ArrayList<String>());
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
