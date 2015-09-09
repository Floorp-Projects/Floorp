/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.home.BrowserSearch.OnEditSuggestionListener;
import org.mozilla.gecko.home.BrowserSearch.OnSearchListener;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.widget.AnimatedHeightLayout;
import org.mozilla.gecko.widget.FaviconView;
import org.mozilla.gecko.widget.FlowLayout;

import android.database.Cursor;
import android.content.ContentResolver;
import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.AlphaAnimation;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.EnumSet;

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
                        final String extras = "engine." + (String) v.getTag();
                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.SUGGESTION, extras);
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
        historyIcon.setVisibility(isUserSavedSearch ? View.VISIBLE: View.GONE);

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
        // is instead initially set in updateFromSearchEngine.
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

    private void bindSuggestionView(String suggestion, boolean animate, int recycledSuggestionCount, Integer previousSuggestionChildIndex, boolean isUserSavedSearch){
        final View suggestionItem;

        // Reuse suggestion views from recycled view, if possible.
        if (previousSuggestionChildIndex + 1 < recycledSuggestionCount) {
            suggestionItem = mSuggestionView.getChildAt(previousSuggestionChildIndex + 1);
            suggestionItem.setVisibility(View.VISIBLE);
        } else {
            suggestionItem = mInflater.inflate(R.layout.suggestion_item, null);

            suggestionItem.setOnClickListener(mClickListener);
            suggestionItem.setOnLongClickListener(mLongClickListener);

            // Store the position of the suggestion for telemetry.
            suggestionItem.setTag(String.valueOf(previousSuggestionChildIndex));

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

    private void updateFromSavedSearches(String searchTerm, boolean animate, int suggestionCounter, int recycledSuggestionCount) {
        final ContentResolver cr = getContext().getContentResolver();

        String[] columns = new String[] { SearchHistory.QUERY };
        String actualQuery = SearchHistory.QUERY + " LIKE ?";
        String[] queryArgs = new String[] { '%' + searchTerm + '%' };
        String sortOrderAndLimit = SearchHistory.DATE + " DESC LIMIT 4";

        final Cursor c = cr.query(SearchHistory.CONTENT_URI, columns, actualQuery, queryArgs, sortOrderAndLimit);

        if (c == null) {
            return;
        }
        try {
            if (c.moveToFirst()) {
                final int searchColumn = c.getColumnIndexOrThrow(SearchHistory.QUERY);
                do {
                    final String savedSearch = c.getString(searchColumn);
                    bindSuggestionView(savedSearch, animate, recycledSuggestionCount, suggestionCounter, true);
                    ++suggestionCounter;
                } while (c.moveToNext());
            }
        } finally {
            c.close();
        }
        hideRecycledSuggestions(suggestionCounter, recycledSuggestionCount);
    }

    private int updateFromSearchEngine(SearchEngine searchEngine, boolean animate, int recycledSuggestionCount) {
        // Update search engine reference.
        mSearchEngine = searchEngine;

        // Set the search engine icon (e.g., Google) for the row.
        mIconView.updateAndScaleImage(mSearchEngine.getIcon(), mSearchEngine.getEngineIdentifier());

        // Set the initial content description.
        setDescriptionOnSuggestion(mUserEnteredTextView, mUserEnteredTextView.getText().toString());

        int suggestionCounter = 0;
        // Apply Search Engine's suggestions
        for (String suggestion : mSearchEngine.getSuggestions()) {
            bindSuggestionView(suggestion, animate, recycledSuggestionCount, suggestionCounter, false);
            ++suggestionCounter;
        }

        hideRecycledSuggestions(suggestionCounter, recycledSuggestionCount);

        // Make sure mSelectedView is still valid.
        if (mSelectedView >= mSuggestionView.getChildCount()) {
            mSelectedView = mSuggestionView.getChildCount() - 1;
        }

        return suggestionCounter;
    }

    public void updateSuggestions(boolean suggestionsEnabled, SearchEngine searchEngine, String searchTerm, boolean animate) {
        // This can be called before the opt-in permission prompt is shown or set. Check first.
        if (suggestionsEnabled) {
            final int recycledSuggestionCount = mSuggestionView.getChildCount();
            final int suggestionViewCount = updateFromSearchEngine(searchEngine, animate, recycledSuggestionCount);
            if (AppConstants.NIGHTLY_BUILD) {
                updateFromSavedSearches(searchTerm, animate, suggestionViewCount, recycledSuggestionCount);
            }
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
