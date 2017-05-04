/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.support.annotation.NonNull;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.search.SearchEngine;
import org.mozilla.gecko.search.SearchEngineManager;
import org.mozilla.gecko.search.SearchEngineManager.SearchEngineCallback;
import org.mozilla.search.autocomplete.SearchBar;
import org.mozilla.search.autocomplete.SuggestionsFragment;

import android.content.AsyncQueryHandler;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.Interpolator;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;

/**
 * The main entrance for the Android search intent.
 * <p/>
 * State management is delegated to child fragments. Fragments communicate
 * with each other by passing messages through this activity.
 */
public class SearchActivity extends Locales.LocaleAwareFragmentActivity
        implements AcceptsSearchQuery, SearchEngineCallback {

    private static final String LOGTAG = "GeckoSearchActivity";

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

    @NonNull
    private SearchEngineManager searchEngineManager; // Contains reference to Context - DO NOT LEAK!

    // Only accessed on the main thread.
    private SearchEngine engine;

    private SuggestionsFragment suggestionsFragment;
    private PostSearchFragment postSearchFragment;

    private AsyncQueryHandler queryHandler;

    // Main views in layout.
    private SearchBar searchBar;
    private View preSearch;
    private View postSearch;

    private View settingsButton;

    private View suggestions;

    private static final int SUGGESTION_TRANSITION_DURATION = 300;
    private static final Interpolator SUGGESTION_TRANSITION_INTERPOLATOR =
            new AccelerateDecelerateInterpolator();

    // View used for suggestion animation.
    private View animationCard;

    // Suggestion card background padding.
    private int cardPaddingX;
    private int cardPaddingY;

    /**
     * An empty implementation of AsyncQueryHandler to avoid the "HandlerLeak" warning from Android
     * Lint. See also {@see org.mozilla.gecko.util.WeakReferenceHandler}.
     */
    private static class AsyncQueryHandlerImpl extends AsyncQueryHandler {
        public AsyncQueryHandlerImpl(final ContentResolver that) {
            super(that);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        GeckoAppShell.ensureCrashHandling();

        super.onCreate(savedInstanceState);
        setContentView(R.layout.search_activity_main);

        suggestionsFragment = (SuggestionsFragment) getSupportFragmentManager().findFragmentById(R.id.suggestions);
        postSearchFragment = (PostSearchFragment)  getSupportFragmentManager().findFragmentById(R.id.postsearch);

        searchEngineManager = new SearchEngineManager(this, Distribution.init(getApplicationContext()));
        searchEngineManager.setChangeCallback(this);

        // Initialize the fragments with the selected search engine.
        searchEngineManager.getEngine(this);

        queryHandler = new AsyncQueryHandlerImpl(getContentResolver());

        searchBar = (SearchBar) findViewById(R.id.search_bar);
        searchBar.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                setEditState(EditState.EDITING);
            }
        });

        searchBar.setTextListener(new SearchBar.TextListener() {
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

            @Override
            public void onFocusChange(boolean hasFocus) {
                setEditState(hasFocus ? EditState.EDITING : EditState.WAITING);
            }
        });

        preSearch = findViewById(R.id.presearch);
        postSearch = findViewById(R.id.postsearch);

        settingsButton = findViewById(R.id.settings_button);

        // Apply click handler to settings button.
        settingsButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startActivity(new Intent(SearchActivity.this, SearchPreferenceActivity.class));
            }
        });

        suggestions = findViewById(R.id.suggestions);

        animationCard = findViewById(R.id.animation_card);

        cardPaddingX = getResources().getDimensionPixelSize(R.dimen.search_row_padding);
        cardPaddingY = getResources().getDimensionPixelSize(R.dimen.search_row_padding);

        if (savedInstanceState != null) {
            setSearchState(SearchState.valueOf(savedInstanceState.getString(KEY_SEARCH_STATE)));
            setEditState(EditState.valueOf(savedInstanceState.getString(KEY_EDIT_STATE)));

            final String query = savedInstanceState.getString(KEY_QUERY);
            searchBar.setText(query);

            // If we're in the postsearch state, we need to re-do the query.
            if (searchState == SearchState.POSTSEARCH) {
                startSearch(query);
            }
        } else {
            // If there isn't a state to restore, the activity will start in the presearch state,
            // and we should enter editing mode to bring up the keyboard.
            setEditState(EditState.EDITING);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        searchEngineManager.unregisterListeners();
        engine = null;
        suggestionsFragment = null;
        postSearchFragment = null;
        queryHandler = null;
        searchBar = null;
        preSearch = null;
        postSearch = null;
        settingsButton = null;
        suggestions = null;
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

        // Enter editing mode and reset the query. We must reset the query after entering
        // edit mode in order for the suggestions to update.
        setEditState(EditState.EDITING);
        searchBar.setText("");
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putString(KEY_SEARCH_STATE, searchState.toString());
        outState.putString(KEY_EDIT_STATE, editState.toString());
        outState.putString(KEY_QUERY, searchBar.getText());
    }

    @Override
    public void onSuggest(String query) {
        searchBar.setText(query);
    }

    @Override
    public void onSearch(String query) {
        onSearch(query, null);
    }

    @Override
    public void onSearch(String query, SuggestionAnimation suggestionAnimation) {
        storeQuery(query);

        try {
            //BrowserHealthRecorder.recordSearchDelayed("activity", engine.getIdentifier());
        } catch (Exception e) {
            // This should never happen: it'll only throw if the
            // search location is wrong. But let's not tempt fate.
            Log.w(LOGTAG, "Unable to record search.");
        }

        startSearch(query);

        if (suggestionAnimation != null) {
            searchBar.setText(query);
            // Animate the suggestion card if start bounds are specified.
            animateSuggestion(suggestionAnimation);
        } else {
            // Otherwise immediately switch to the results view.
            setEditState(EditState.WAITING);
            setSearchState(SearchState.POSTSEARCH);
        }
    }

    @Override
    public void onQueryChange(String query) {
        searchBar.setText(query);
    }

    private void startSearch(final String query) {
        if (engine != null) {
            postSearchFragment.startSearch(engine, query);
            return;
        }

        // engine will only be null if startSearch is called before the getEngine
        // call in onCreate is completed.
        searchEngineManager.getEngine(new SearchEngineCallback() {
            @Override
            public void execute(SearchEngine engine) {
                // TODO: If engine is null, we should show an error message.
                if (engine != null) {
                    postSearchFragment.startSearch(engine, query);
                }
            }
        });
    }

    /**
     * This method is called when we fetch the current engine in onCreate,
     * as well as whenever the current engine changes. This method will only
     * ever be called on the main thread.
     *
     * @param engine The current search engine.
     */
    @Override
    public void execute(SearchEngine engine) {
        // TODO: If engine is null, we should show an error message.
        if (engine == null) {
            return;
        }
        this.engine = engine;
        suggestionsFragment.setEngine(engine);
        searchBar.setEngine(engine);
    }

    /**
     * Animates search suggestion item to fill the results view area.
     *
     * @param suggestionAnimation
     */
    private void animateSuggestion(final SuggestionAnimation suggestionAnimation) {
        final Rect startBounds = suggestionAnimation.getStartBounds();
        final Rect endBounds = new Rect();
        animationCard.getGlobalVisibleRect(endBounds, null);

        // Vertically translate the animated card to align with the start bounds.
        final float cardStartY = startBounds.centerY() - endBounds.centerY();

        // Account for card background padding when calculating start scale.
        final float startScaleX = (float) (startBounds.width() - cardPaddingX * 2) / endBounds.width();
        final float startScaleY = (float) (startBounds.height() - cardPaddingY * 2) / endBounds.height();

        animationCard.setVisibility(View.VISIBLE);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(
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
                // Don't do anything if the activity is destroyed before the animation ends.
                if (searchEngineManager == null) {
                    return;
                }

                setEditState(EditState.WAITING);
                setSearchState(SearchState.POSTSEARCH);

                // We need to manually clear the animation for the views to be hidden on gingerbread.
                animationCard.clearAnimation();
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

        updateSettingsButtonVisibility();

        searchBar.setActive(editState == EditState.EDITING);
        suggestions.setVisibility(editState == EditState.EDITING ? View.VISIBLE : View.INVISIBLE);
    }

    private void setSearchState(SearchState searchState) {
        if (this.searchState == searchState) {
            return;
        }
        this.searchState = searchState;

        updateSettingsButtonVisibility();

        preSearch.setVisibility(searchState == SearchState.PRESEARCH ? View.VISIBLE : View.INVISIBLE);
        postSearch.setVisibility(searchState == SearchState.POSTSEARCH ? View.VISIBLE : View.INVISIBLE);
    }

    private void updateSettingsButtonVisibility() {
        // Show button on launch screen when keyboard is down.
        if (searchState == SearchState.PRESEARCH && editState == EditState.WAITING) {
            settingsButton.setVisibility(View.VISIBLE);
        } else {
            settingsButton.setVisibility(View.INVISIBLE);
        }
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
