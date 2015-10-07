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
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.widget.AnimatedHeightLayout;
import org.mozilla.gecko.widget.FaviconView;
import org.mozilla.gecko.widget.FlowLayout;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.AlphaAnimation;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.EnumSet;
import java.util.List;

class SearchEngineRow extends AnimatedHeightLayout {
    // Duration for fade-in animation
    private static final int ANIMATION_DURATION = 250;

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
                    mSearchListener.onSearch(mSearchEngine, suggestion);
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
        mSearchHistorySuggestionIcon = DrawableUtil.tintDrawable(getContext(), R.drawable.icon_most_recent_empty, R.color.tabs_tray_icon_grey);

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

    private void setSuggestionOnView(View v, String suggestion, boolean isUserSavedSearch) {
        final ImageView historyIcon = (ImageView) v.findViewById(R.id.suggestion_item_icon);
        if (isUserSavedSearch) {
            historyIcon.setImageDrawable(mSearchHistorySuggestionIcon);
            historyIcon.setVisibility(View.VISIBLE);
        } else {
            historyIcon.setVisibility(View.GONE);
        }

        final TextView suggestionText = (TextView) v.findViewById(R.id.suggestion_text);
        suggestionText.setText(suggestion);
        setDescriptionOnSuggestion(suggestionText, suggestion);
    }

    /**
     * Perform a search for the user-entered term.
     */
    public void performUserEnteredSearch() {
        String searchTerm = getSuggestionTextFromView(mUserEnteredView);
        if (mSearchListener != null) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.SUGGESTION, "user");
            mSearchListener.onSearch(mSearchEngine, searchTerm);
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

    private void bindSuggestionView(String suggestion, boolean animate, int recycledSuggestionCount, Integer previousSuggestionChildIndex, boolean isUserSavedSearch, String telemetryTag){
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

        if (animate) {
            AlphaAnimation anim = new AlphaAnimation(0, 1);
            anim.setDuration(ANIMATION_DURATION);
            anim.setStartOffset(previousSuggestionChildIndex * ANIMATION_DURATION);
            suggestionItem.startAnimation(anim);
        }
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
     * @param savedSuggestions The List to iterate over for saved search suggestions to display
     * @param suggestionCounter global index of where to start adding suggestion "buttons" in the search engine row
     * @param animate whether or not to animate suggestions for visual polish
     * @param recycledSuggestionCount How many suggestion "button" views we could recycle from previous calls
     */
    private void updateFromSavedSearches(List<String> savedSuggestions, boolean animate, int suggestionCounter, int recycledSuggestionCount) {
        if (savedSuggestions == null || savedSuggestions.isEmpty()) {
            return;
        }

        final int historyStartIndex = suggestionCounter;
        for (String suggestion : savedSuggestions) {
            String telemetryTag = "history." + (suggestionCounter - historyStartIndex);
            bindSuggestionView(suggestion, animate, recycledSuggestionCount, suggestionCounter, true, telemetryTag);
            ++suggestionCounter;
        }

        hideRecycledSuggestions(suggestionCounter, recycledSuggestionCount);
    }

    /**
     * Displays suggestions supplied by the search engine, relative to number of suggestions from search history.
     *
     * @param animate whether or not to animate suggestions for visual polish
     * @param recycledSuggestionCount How many suggestion "button" views we could recycle from previous calls
     * @param savedSuggestionCount how many saved searches this searchTerm has
     * @return the global count of how many suggestions have been bound/shown in the search engine row
     */
    private int updateFromSearchEngine(boolean animate, int recycledSuggestionCount, int savedSuggestionCount) {
        int maxSuggestions = mMaxSearchSuggestions;
        // If there are less than max saved searches on phones, fill the space with more search engine suggestions
        if (!HardwareUtils.isTablet() && savedSuggestionCount < mMaxSavedSuggestions) {
            maxSuggestions += mMaxSavedSuggestions - savedSuggestionCount;
        }

        int suggestionCounter = 0;
        for (String suggestion : mSearchEngine.getSuggestions()) {
            if (suggestionCounter == maxSuggestions) {
                break;
            }

            // Since the search engine suggestions are listed first, we can use suggestionCounter to get their relative positions for telemetry
            String telemetryTag = "engine." + suggestionCounter;
            bindSuggestionView(suggestion, animate, recycledSuggestionCount, suggestionCounter, false, telemetryTag);
            ++suggestionCounter;
        }

        hideRecycledSuggestions(suggestionCounter, recycledSuggestionCount);

        // Make sure mSelectedView is still valid.
        if (mSelectedView >= mSuggestionView.getChildCount()) {
            mSelectedView = mSuggestionView.getChildCount() - 1;
        }

        return suggestionCounter;
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
     * @param searchHistorySuggestions search history suggestions
     * @param animate whether or not to use animations
     **/
    public void updateSuggestions(boolean searchSuggestionsEnabled, SearchEngine searchEngine, List<String> searchHistorySuggestions, boolean animate) {
        mSearchEngine = searchEngine;
        // Set the search engine icon (e.g., Google) for the row.
        mIconView.updateAndScaleImage(mSearchEngine.getIcon(), mSearchEngine.getEngineIdentifier());
        // Set the initial content description.
        setDescriptionOnSuggestion(mUserEnteredTextView, mUserEnteredTextView.getText().toString());

        final int recycledSuggestionCount = mSuggestionView.getChildCount();
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(getContext());
        final boolean savedSearchesEnabled = prefs.getBoolean(GeckoPreferences.PREFS_HISTORY_SAVED_SEARCH, true);

        if (searchSuggestionsEnabled && savedSearchesEnabled) {
            final int savedSearchCount = (searchHistorySuggestions != null) ? searchHistorySuggestions.size() : 0;
            final int suggestionViewCount = updateFromSearchEngine(animate, recycledSuggestionCount, savedSearchCount);
            updateFromSavedSearches(searchHistorySuggestions, animate, suggestionViewCount, recycledSuggestionCount);

        } else if (savedSearchesEnabled) {
            updateFromSavedSearches(searchHistorySuggestions, animate, 0, recycledSuggestionCount);
        } else if (searchSuggestionsEnabled) {
            updateFromSearchEngine(animate, recycledSuggestionCount, 0);
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
