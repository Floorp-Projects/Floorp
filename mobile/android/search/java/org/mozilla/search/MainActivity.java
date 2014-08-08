/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.content.AsyncQueryHandler;
import android.content.ContentValues;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.TextView;

import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.search.autocomplete.ClearableEditText;
import org.mozilla.search.autocomplete.SuggestionsFragment;

/**
 * The main entrance for the Android search intent.
 * <p/>
 * State management is delegated to child fragments. Fragments communicate
 * with each other by passing messages through this activity.
 */
public class MainActivity extends FragmentActivity implements AcceptsSearchQuery {

    private static final String KEY_SEARCH_STATE = "search_state";
    private static final String KEY_EDIT_STATE = "edit_state";
    private static final String KEY_QUERY = "query";

    static enum SearchState {
        PRESEARCH,
        POSTSEARCH
    }

    static enum EditState {
        WAITING,
        EDITING
    }

    // Default states when activity is created.
    private SearchState searchState = SearchState.PRESEARCH;
    private EditState editState = EditState.WAITING;

    private AsyncQueryHandler queryHandler;

    // Main views in layout.
    private ClearableEditText editText;
    private View preSearch;
    private View postSearch;

    private View suggestionsContainer;
    private SuggestionsFragment suggestionsFragment;

    private static final int SUGGESTION_TRANSITION_DURATION = 300;
    private static final Interpolator SUGGESTION_TRANSITION_INTERPOLATOR =
            new AccelerateDecelerateInterpolator();

    // Views used for suggestion animation.
    private TextView animationText;
    private View animationCard;

    // Suggestion card background padding.
    private int cardPaddingX;
    private int cardPaddingY;

    // Vertical translation of suggestion animation text to align with the search bar.
    private int textEndY;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.search_activity_main);

        queryHandler = new AsyncQueryHandler(getContentResolver()) {};

        editText = (ClearableEditText) findViewById(R.id.search_edit_text);
        editText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                setEditState(EditState.EDITING);
            }
        });

        editText.setTextListener(new ClearableEditText.TextListener() {
            @Override
            public void onChange(String text) {
                // Only load suggestions if we're in edit mode.
                if (editState == EditState.EDITING) {
                    suggestionsFragment.loadSuggestions(text);
                }
            }

            @Override
            public void onSubmit(String text) {
                // Don't submit an empty query.
                final String trimmedQuery = text.trim();
                if (!TextUtils.isEmpty(trimmedQuery)) {
                    onSearch(trimmedQuery);
                }
            }
        });

        preSearch = findViewById(R.id.presearch);
        postSearch = findViewById(R.id.postsearch);

        suggestionsContainer = findViewById(R.id.suggestions_container);
        suggestionsFragment = (SuggestionsFragment) getSupportFragmentManager().findFragmentById(R.id.suggestions);

        // Dismiss edit mode when the user taps outside of the suggestions.
        findViewById(R.id.suggestions_container).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                setEditState(EditState.WAITING);
            }
        });

        animationText = (TextView) findViewById(R.id.animation_text);
        animationCard = findViewById(R.id.animation_card);

        cardPaddingX = getResources().getDimensionPixelSize(R.dimen.card_background_padding_x);
        cardPaddingY = getResources().getDimensionPixelSize(R.dimen.card_background_padding_y);
        textEndY = getResources().getDimensionPixelSize(R.dimen.animation_text_translation_y);

        if (savedInstanceState != null) {
            setSearchState(SearchState.valueOf(savedInstanceState.getString(KEY_SEARCH_STATE)));
            setEditState(EditState.valueOf(savedInstanceState.getString(KEY_EDIT_STATE)));

            final String query = savedInstanceState.getString(KEY_QUERY);
            editText.setText(query);

            // If we're in the postsearch state, we need to re-do the query.
            if (searchState == SearchState.POSTSEARCH) {
                ((PostSearchFragment) getSupportFragmentManager().findFragmentById(R.id.postsearch))
                        .startSearch(query);
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        queryHandler = null;
        editText = null;
        preSearch = null;
        postSearch = null;
        suggestionsFragment = null;
        suggestionsContainer = null;
        animationText = null;
        animationCard = null;
    }

    @Override
    protected void onStart() {
        super.onStart();
        Telemetry.startUISession(TelemetryContract.Session.SEARCH_ACTIVITY);
    }

    @Override
    protected void onStop() {
        super.onStop();
        Telemetry.stopUISession(TelemetryContract.Session.SEARCH_ACTIVITY);
    }

    @Override
    public void onNewIntent(Intent intent) {
        // Reset the activity in the presearch state if it was launched from a new intent.
        setSearchState(SearchState.PRESEARCH);

        // Also clear any existing search term.
        editText.setText("");
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putString(KEY_SEARCH_STATE, searchState.toString());
        outState.putString(KEY_EDIT_STATE, editState.toString());
        outState.putString(KEY_QUERY, editText.getText());
    }

    @Override
    public void onSuggest(String query) {
       editText.setText(query);
    }

    @Override
    public void onSearch(String query) {
        onSearch(query, null);
    }

    @Override
    public void onSearch(String query, SuggestionAnimation suggestionAnimation) {
        storeQuery(query);

        ((PostSearchFragment) getSupportFragmentManager().findFragmentById(R.id.postsearch))
                .startSearch(query);

        if (suggestionAnimation != null) {
            // Animate the suggestion card if start bounds are specified.
            animateSuggestion(query, suggestionAnimation);
        } else {
            // Otherwise immediately switch to the results view.
            setEditState(EditState.WAITING);
            setSearchState(SearchState.POSTSEARCH);
        }
    }

    /**
     * Animates search suggestion to search bar. This animation has 2 main parts:
     *
     *   1) Vertically translate query text from suggestion card to search bar.
     *   2) Expand suggestion card to fill the results view area.
     *
     * @param query
     * @param suggestionAnimation
     */
    private void animateSuggestion(final String query, final SuggestionAnimation suggestionAnimation) {
        animationText.setText(query);

        final Rect startBounds = suggestionAnimation.getStartBounds();
        final Rect endBounds = new Rect();
        animationCard.getGlobalVisibleRect(endBounds, null);

        // Vertically translate the animated card to align with the start bounds.
        final float cardStartY = startBounds.centerY() - endBounds.centerY();

        // Account for card background padding when calculating start scale.
        final float startScaleX = (float) (startBounds.width() - cardPaddingX * 2) / endBounds.width();
        final float startScaleY = (float) (startBounds.height() - cardPaddingY * 2) / endBounds.height();

        animationText.setVisibility(View.VISIBLE);
        animationCard.setVisibility(View.VISIBLE);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(
                ObjectAnimator.ofFloat(animationText, "translationY", startBounds.top, textEndY),
                ObjectAnimator.ofFloat(animationCard, "translationY", cardStartY, 0),
                ObjectAnimator.ofFloat(animationCard, "alpha", 0.5f, 1),
                ObjectAnimator.ofFloat(animationCard, "scaleX", startScaleX, 1f),
                ObjectAnimator.ofFloat(animationCard, "scaleY", startScaleY, 1f)
        );

        set.addListener(new Animator.AnimatorListener() {
            @Override
            public void onAnimationStart(Animator animation) {
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                setEditState(EditState.WAITING);
                setSearchState(SearchState.POSTSEARCH);

                editText.setText(query);

                // We need to manually clear the animation for the views to be hidden on gingerbread.
                animationText.clearAnimation();
                animationCard.clearAnimation();

                animationText.setVisibility(View.INVISIBLE);
                animationCard.setVisibility(View.INVISIBLE);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
            }

            @Override
            public void onAnimationRepeat(Animator animation) {
            }
        });

        set.setDuration(SUGGESTION_TRANSITION_DURATION);
        set.setInterpolator(SUGGESTION_TRANSITION_INTERPOLATOR);

        set.start();
    }

    private void setEditState(EditState editState) {
        if (this.editState == editState) {
            return;
        }
        this.editState = editState;

        editText.setActive(editState == EditState.EDITING);
        suggestionsContainer.setVisibility(editState == EditState.EDITING ? View.VISIBLE : View.INVISIBLE);
    }

    private void setSearchState(SearchState searchState) {
        if (this.searchState == searchState) {
            return;
        }
        this.searchState = searchState;

        preSearch.setVisibility(searchState == SearchState.PRESEARCH ? View.VISIBLE : View.INVISIBLE);
        postSearch.setVisibility(searchState == SearchState.POSTSEARCH ? View.VISIBLE : View.INVISIBLE);
    }

    @Override
    public void onBackPressed() {
        if (editState == EditState.EDITING) {
            setEditState(EditState.WAITING);
        } else if (searchState == SearchState.POSTSEARCH) {
            setSearchState(SearchState.PRESEARCH);
        } else {
            super.onBackPressed();
        }
    }

    /**
     * Store the search query in Fennec's search history database.
     */
    private void storeQuery(String query) {
        final ContentValues cv = new ContentValues();
        cv.put(SearchHistory.QUERY, query);
        // Setting 0 for the token, since we only have one type of insert.
        // Setting null for the cookie, since we don't handle the result of the insert.
        queryHandler.startInsert(0, null, SearchHistory.CONTENT_URI, cv);
    }
}
