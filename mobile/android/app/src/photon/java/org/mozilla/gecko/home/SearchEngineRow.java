/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.home.BrowserSearch.OnEditSuggestionListener;
import org.mozilla.gecko.home.BrowserSearch.OnSearchListener;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.widget.FaviconView;
import org.mozilla.gecko.widget.FlowLayout;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.graphics.Typeface;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.Iterator;
import java.util.List;
import java.util.regex.Pattern;

class SearchEngineRow extends RelativeLayout {

    // Inner views
    private final FlowLayout mSuggestionView;
    private final FaviconView mIconView;
    private final LinearLayout mUserEnteredView;
    private final TextView mUserEnteredTextView;

    // Inflater used when updating from suggestions
    private final LayoutInflater mInflater;

    // Search engine associated with this view
    private SearchEngine mSearchEngine;

    // Event listeners for suggestion views
    private final OnClickListener mClickListener;
    private final OnLongClickListener mLongClickListener;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    // On search listener
    private OnSearchListener mSearchListener;

    // On edit suggestion listener
    private OnEditSuggestionListener mEditSuggestionListener;

    // Selected suggestion view
    private int mSelectedView;

    // android:backgroundTint only works in Android 21 and higher so we can't do this statically in the xml
    private Drawable mSearchHistorySuggestionIcon;

    // Maximums for suggestions
    private int mMaxSavedSuggestions;
    private int mMaxSearchSuggestions;

    private final List<Integer> mOccurrences = new ArrayList<>();

    public SearchEngineRow(Context context) {
        this(context, null);
    }

    public SearchEngineRow(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SearchEngineRow(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        mClickListener = new OnClickListener() {
            @Override
            public void onClick(View v) {
                final String suggestion = getSuggestionTextFromView(v);

                // If we're not clicking the user-entered view (the first suggestion item)
                // and the search matches a URL pattern, go to that URL. Otherwise, do a
                // search for the term.
                if (v != mUserEnteredView && !StringUtils.isSearchQuery(suggestion, true)) {
                    if (mUrlOpenListener != null) {
                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.SUGGESTION, "url");

                        mUrlOpenListener.onUrlOpen(suggestion, EnumSet.noneOf(OnUrlOpenListener.Flags.class));
                    }
                } else if (mSearchListener != null) {
                    if (v == mUserEnteredView) {
                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.SUGGESTION, "user");
                    } else {
                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.SUGGESTION, (String) v.getTag());
                    }
                    mSearchListener.onSearch(mSearchEngine, suggestion, TelemetryContract.Method.SUGGESTION);
                }
            }
        };

        mLongClickListener = new OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                if (mEditSuggestionListener != null) {
                    final String suggestion = getSuggestionTextFromView(v);
                    mEditSuggestionListener.onEditSuggestion(suggestion);
                    return true;
                }

                return false;
            }
        };

        mInflater = LayoutInflater.from(context);
        mInflater.inflate(R.layout.search_engine_row, this);

        mSuggestionView = (FlowLayout) findViewById(R.id.suggestion_layout);
        mIconView = (FaviconView) findViewById(R.id.suggestion_icon);

        // User-entered search term is first suggestion
        mUserEnteredView = (LinearLayout) findViewById(R.id.suggestion_user_entered);
        mUserEnteredView.setOnClickListener(mClickListener);

        mUserEnteredTextView = (TextView) findViewById(R.id.suggestion_text);
        mSearchHistorySuggestionIcon = DrawableUtil.tintDrawableWithColorRes(getContext(), R.drawable.icon_most_recent_empty, R.color.tabs_tray_icon_grey);

        // Suggestion limits
        mMaxSavedSuggestions = getResources().getInteger(R.integer.max_saved_suggestions);
        mMaxSearchSuggestions = getResources().getInteger(R.integer.max_search_suggestions);
    }

    private void setDescriptionOnSuggestion(View v, String suggestion) {
        v.setContentDescription(getResources().getString(R.string.suggestion_for_engine,
                                                         mSearchEngine.name, suggestion));
    }

    private String getSuggestionTextFromView(View v) {
        final TextView suggestionText = (TextView) v.findViewById(R.id.suggestion_text);
        return suggestionText.getText().toString();
    }

    /**
     * Finds all occurrences of pattern in string and returns a list of the starting indices
     * of each occurrence.
     *
     * @param pattern The pattern that is searched for
     * @param string The string where we search for the pattern
     */
    private void refreshOccurrencesWith(String pattern, String string) {
        mOccurrences.clear();

        // Don't try to search for an empty string - String.indexOf will return 0, which would result
        // in us iterating with lastIndexOfMatch = 0, which eventually results in an OOM.
        if (TextUtils.isEmpty(pattern)) {
            return;
        }

        final int patternLength = pattern.length();

        int indexOfMatch = 0;
        int lastIndexOfMatch = 0;
        while (indexOfMatch != -1) {
            indexOfMatch = string.indexOf(pattern, lastIndexOfMatch);
            lastIndexOfMatch = indexOfMatch + patternLength;
            if (indexOfMatch != -1) {
                mOccurrences.add(indexOfMatch);
            }
        }
    }

    /**
     * Sets the content for the suggestion view.
     *
     * If the suggestion doesn't contain mUserSearchTerm, nothing is made bold.
     * All instances of mUserSearchTerm in the suggestion are not bold.
     *
     * @param v The View that needs to be populated
     * @param suggestion The suggestion text that will be placed in the view
     * @param isUserSavedSearch whether the suggestion is from history or not
     */
    private void setSuggestionOnView(View v, String suggestion, boolean isUserSavedSearch) {
        final ImageView historyIcon = (ImageView) v.findViewById(R.id.suggestion_item_icon);
        if (isUserSavedSearch) {
            historyIcon.setImageDrawable(mSearchHistorySuggestionIcon);
            historyIcon.setVisibility(View.VISIBLE);
        } else {
            historyIcon.setVisibility(View.GONE);
        }

        final TextView suggestionText = (TextView) v.findViewById(R.id.suggestion_text);
        final String searchTerm = getSuggestionTextFromView(mUserEnteredView);
        final int searchTermLength = searchTerm.length();
        refreshOccurrencesWith(searchTerm, suggestion);
        if (mOccurrences.size() > 0) {
            final SpannableStringBuilder sb = new SpannableStringBuilder(suggestion);
            int nextStartSpanIndex = 0;
            // Done to make sure that the stretch of text after the last occurrence, till the end of the suggestion, is made bold
            mOccurrences.add(suggestion.length());
            for (int occurrence : mOccurrences) {
                // Even though they're the same style, SpannableStringBuilder will interpret there as being only one Span present if we re-use a StyleSpan
                StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
                sb.setSpan(boldSpan, nextStartSpanIndex, occurrence, Spannable.SPAN_INCLUSIVE_INCLUSIVE);
                nextStartSpanIndex = occurrence + searchTermLength;
            }
            mOccurrences.clear();
            suggestionText.setText(sb);
        } else {
            suggestionText.setText(suggestion);
        }

        setDescriptionOnSuggestion(suggestionText, suggestion);
    }

    /**
     * Perform a search for the user-entered term.
     */
    public void performUserEnteredSearch() {
        String searchTerm = getSuggestionTextFromView(mUserEnteredView);
        if (mSearchListener != null) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.SUGGESTION, "user");
            mSearchListener.onSearch(mSearchEngine, searchTerm, TelemetryContract.Method.SUGGESTION);
        }
    }

    public void setSearchTerm(String searchTerm) {
        mUserEnteredTextView.setText(searchTerm);

        // mSearchEngine is not set in the first call to this method; the content description
        // is instead initially set in updateSuggestions().
        if (mSearchEngine != null) {
            setDescriptionOnSuggestion(mUserEnteredTextView, searchTerm);
        }
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    public void setOnSearchListener(OnSearchListener listener) {
        mSearchListener = listener;
    }

    public void setOnEditSuggestionListener(OnEditSuggestionListener listener) {
        mEditSuggestionListener = listener;
    }

    private void bindSuggestionView(String suggestion, int recycledSuggestionCount, Integer previousSuggestionChildIndex, boolean isUserSavedSearch, String telemetryTag) {
        final View suggestionItem;

        // Reuse suggestion views from recycled view, if possible.
        if (previousSuggestionChildIndex + 1 < recycledSuggestionCount) {
            suggestionItem = mSuggestionView.getChildAt(previousSuggestionChildIndex + 1);
            suggestionItem.setVisibility(View.VISIBLE);
        } else {
            suggestionItem = mInflater.inflate(R.layout.suggestion_item, null);

            suggestionItem.setOnClickListener(mClickListener);
            suggestionItem.setOnLongClickListener(mLongClickListener);

            suggestionItem.setTag(telemetryTag);

            mSuggestionView.addView(suggestionItem);
        }

        setSuggestionOnView(suggestionItem, suggestion, isUserSavedSearch);
    }

    private void hideRecycledSuggestions(int lastVisibleChildIndex, int recycledSuggestionCount) {
        // Hide extra suggestions that have been recycled.
        for (int i = lastVisibleChildIndex + 1; i < recycledSuggestionCount; ++i) {
            mSuggestionView.getChildAt(i).setVisibility(View.GONE);
        }
    }

    /**
     * Displays search suggestions from previous searches.
     *
     * @param savedSuggestions The List to iterate over for saved search suggestions to display. This function does not
     *                         enforce a ui maximum or filter. It will show all the suggestions in this list.
     * @param suggestionStartIndex global index of where to start adding suggestion "buttons" in the search engine row. Also
     *                             acts as a counter for total number of suggestions visible.
     * @param recycledSuggestionCount How many suggestion "button" views we could recycle from previous calls
     */
    private void updateFromSavedSearches(List<String> savedSuggestions, int suggestionStartIndex, int recycledSuggestionCount) {
        if (savedSuggestions == null || savedSuggestions.isEmpty()) {
            hideRecycledSuggestions(suggestionStartIndex, recycledSuggestionCount);
            return;
        }

        final int numSavedSearches = savedSuggestions.size();
        int indexOfPreviousSuggestion = 0;
        for (int i = 0; i < numSavedSearches; i++) {
            String telemetryTag = "history." + i;
            final String suggestion = savedSuggestions.get(i);
            indexOfPreviousSuggestion = suggestionStartIndex + i;
            bindSuggestionView(suggestion, recycledSuggestionCount, indexOfPreviousSuggestion, true, telemetryTag);
        }

        hideRecycledSuggestions(indexOfPreviousSuggestion + 1, recycledSuggestionCount);
    }

    /**
     * Displays suggestions supplied by the search engine, relative to number of suggestions from search history.
     *
     * @param recycledSuggestionCount How many suggestion "button" views we could recycle from previous calls
     * @param savedSuggestionCount how many saved searches this searchTerm has
     * @return the global count of how many suggestions have been bound/shown in the search engine row
     */
    private int updateFromSearchEngine(List<String> searchEngineSuggestions, int recycledSuggestionCount, int savedSuggestionCount) {
        int maxSuggestions = mMaxSearchSuggestions;
        // If there are less than max saved searches on phones, fill the space with more search engine suggestions
        if (!HardwareUtils.isTablet() && savedSuggestionCount < mMaxSavedSuggestions) {
            maxSuggestions += mMaxSavedSuggestions - savedSuggestionCount;
        }

        final int numSearchEngineSuggestions = searchEngineSuggestions.size();
        int relativeIndex;
        for (relativeIndex = 0; relativeIndex < numSearchEngineSuggestions; relativeIndex++) {
            if (relativeIndex == maxSuggestions) {
                break;
            }

            // Since the search engine suggestions are listed first, their relative index is their global index
            String telemetryTag = "engine." + relativeIndex;
            final String suggestion = searchEngineSuggestions.get(relativeIndex);
            bindSuggestionView(suggestion, recycledSuggestionCount, relativeIndex, false, telemetryTag);
        }

        hideRecycledSuggestions(relativeIndex + 1, recycledSuggestionCount);

        // Make sure mSelectedView is still valid.
        if (mSelectedView >= mSuggestionView.getChildCount()) {
            mSelectedView = mSuggestionView.getChildCount() - 1;
        }

        return relativeIndex;
    }

    /**
     * Updates the whole suggestions UI, the search engine UI, suggestions from the default search engine,
     * and suggestions from search history.
     *
     * This can be called before the opt-in permission prompt is shown or set.
     * Even if both suggestion types are disabled, we need to update the search engine, its image, and the content description.
     *
     * @param searchSuggestionsEnabled whether or not suggestions from the default search engine are enabled
     * @param searchEngine the search engine to use throughout the SearchEngineRow class
     * @param rawSearchHistorySuggestions search history suggestions
     **/
    public void updateSuggestions(boolean searchSuggestionsEnabled, SearchEngine searchEngine, @Nullable List<String> rawSearchHistorySuggestions) {
        mSearchEngine = searchEngine;
        // Set the search engine icon (e.g., Google) for the row.

        mIconView.updateAndScaleImage(IconResponse.create(mSearchEngine.getIcon()));
        // Set the initial content description.
        setDescriptionOnSuggestion(mUserEnteredTextView, mUserEnteredTextView.getText().toString());

        final int recycledSuggestionCount = mSuggestionView.getChildCount();
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(getContext());
        final boolean savedSearchesEnabled = prefs.getBoolean(GeckoPreferences.PREFS_HISTORY_SAVED_SEARCH, true);

        // Remove duplicates of search engine suggestions from saved searches.
        List<String> searchHistorySuggestions = (rawSearchHistorySuggestions != null) ? rawSearchHistorySuggestions : new ArrayList<String>();

        // Filter out URLs and long search suggestions
        Iterator<String> searchHistoryIterator = searchHistorySuggestions.iterator();
        while (searchHistoryIterator.hasNext()) {
            final String currentSearchHistory = searchHistoryIterator.next();

            if (currentSearchHistory.length() > 50 || Pattern.matches("^(https?|ftp|file)://.*", currentSearchHistory)) {
                searchHistoryIterator.remove();
            }
        }


        final List<String> searchEngineSuggestions = new ArrayList<>();
        if (searchSuggestionsEnabled) {
            for (String suggestion : searchEngine.getSuggestions()) {
                searchHistorySuggestions.remove(suggestion);
                searchEngineSuggestions.add(suggestion);
            }
        }
        // Make sure the search term itself isn't duplicated. This is more important on phones than tablets where screen
        // space is more precious.
        searchHistorySuggestions.remove(getSuggestionTextFromView(mUserEnteredView));

        // Trim the history suggestions down to the maximum allowed.
        if (searchHistorySuggestions.size() >= mMaxSavedSuggestions) {
            // The second index to subList() is exclusive, so this looks like an off by one error but it is not.
            searchHistorySuggestions = searchHistorySuggestions.subList(0, mMaxSavedSuggestions);
        }
        final int searchHistoryCount = searchHistorySuggestions.size();

        if (searchSuggestionsEnabled && savedSearchesEnabled) {
            final int suggestionViewCount = updateFromSearchEngine(searchEngineSuggestions, recycledSuggestionCount, searchHistoryCount);
            updateFromSavedSearches(searchHistorySuggestions, suggestionViewCount, recycledSuggestionCount);
        } else if (savedSearchesEnabled) {
            updateFromSavedSearches(searchHistorySuggestions, 0, recycledSuggestionCount);
        } else if (searchSuggestionsEnabled) {
            updateFromSearchEngine(searchEngineSuggestions, recycledSuggestionCount, 0);
        } else {
            // The current search term is treated separately from the suggestions list, hence we can
            // recycle ALL suggestion items here. (We always show the current search term, i.e. 1 item,
            // in front of the search engine suggestions and/or the search history.)
            hideRecycledSuggestions(0, recycledSuggestionCount);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, android.view.KeyEvent event) {
        final View suggestion = mSuggestionView.getChildAt(mSelectedView);

        if (event.getAction() != android.view.KeyEvent.ACTION_DOWN) {
            return false;
        }

        switch (event.getKeyCode()) {
        case KeyEvent.KEYCODE_DPAD_RIGHT:
            final View nextSuggestion = mSuggestionView.getChildAt(mSelectedView + 1);
            if (nextSuggestion != null) {
                changeSelectedSuggestion(suggestion, nextSuggestion);
                mSelectedView++;
                return true;
            }
            break;

        case KeyEvent.KEYCODE_DPAD_LEFT:
            final View prevSuggestion = mSuggestionView.getChildAt(mSelectedView - 1);
            if (prevSuggestion != null) {
                changeSelectedSuggestion(suggestion, prevSuggestion);
                mSelectedView--;
                return true;
            }
            break;

        case KeyEvent.KEYCODE_BUTTON_A:
            // TODO: handle long pressing for editing suggestions
            return suggestion.performClick();
        }

        return false;
    }

    private void changeSelectedSuggestion(View oldSuggestion, View newSuggestion) {
        oldSuggestion.setDuplicateParentStateEnabled(false);
        newSuggestion.setDuplicateParentStateEnabled(true);
        oldSuggestion.refreshDrawableState();
        newSuggestion.refreshDrawableState();
    }

    public void onSelected() {
        mSelectedView = 0;
        mUserEnteredView.setDuplicateParentStateEnabled(true);
        mUserEnteredView.refreshDrawableState();
    }

    public void onDeselected() {
        final View suggestion = mSuggestionView.getChildAt(mSelectedView);
        suggestion.setDuplicateParentStateEnabled(false);
        suggestion.refreshDrawableState();
    }
}
