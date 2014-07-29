/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.content.AsyncQueryHandler;
import android.content.ContentValues;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.TextView;

import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;

import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.search.autocomplete.SearchFragment;

/**
 * The main entrance for the Android search intent.
 * <p/>
 * State management is delegated to child fragments. Fragments communicate
 * with each other by passing messages through this activity. The only message passing right
 * now, the only message passing occurs when a user wants to submit a search query. That
 * passes through the onSearch method here.
 */
public class MainActivity extends FragmentActivity implements AcceptsSearchQuery {

    enum State {
        START,
        PRESEARCH,
        POSTSEARCH
    }

    private State state = State.START;
    private AsyncQueryHandler queryHandler;

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
    protected void onCreate(Bundle stateBundle) {
        super.onCreate(stateBundle);
        setContentView(R.layout.search_activity_main);

        queryHandler = new AsyncQueryHandler(getContentResolver()) {};

        animationText = (TextView) findViewById(R.id.animation_text);
        animationCard = findViewById(R.id.animation_card);

        cardPaddingX = getResources().getDimensionPixelSize(R.dimen.card_background_padding_x);
        cardPaddingY = getResources().getDimensionPixelSize(R.dimen.card_background_padding_y);
        textEndY = getResources().getDimensionPixelSize(R.dimen.animation_text_translation_y);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        queryHandler = null;

        animationText = null;
        animationCard = null;
    }

    @Override
    protected void onResume() {
        super.onResume();
        // When the app launches, make sure we're in presearch *always*
        startPresearch();
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
            startPostsearch();
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
                // This is crappy, but right now we need to make sure we call this before
                // setSearchTerm so that we don't kick off a suggestion request.
                suggestionAnimation.onAnimationEnd();

                // TODO: Find a way to do this without needing to access SearchFragment.
                final SearchFragment searchFragment = ((SearchFragment) getSupportFragmentManager().findFragmentById(R.id.search_fragment));
                searchFragment.setSearchTerm(query);

                startPostsearch();

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

    private void startPresearch() {
        if (state != State.PRESEARCH) {
            state = State.PRESEARCH;
            findViewById(R.id.postsearch).setVisibility(View.INVISIBLE);
            findViewById(R.id.presearch).setVisibility(View.VISIBLE);
        }
    }

    private void startPostsearch() {
        if (state != State.POSTSEARCH) {
            state = State.POSTSEARCH;
            findViewById(R.id.presearch).setVisibility(View.INVISIBLE);
            findViewById(R.id.postsearch).setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void onBackPressed() {
        if (state == State.POSTSEARCH) {
            startPresearch();
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
