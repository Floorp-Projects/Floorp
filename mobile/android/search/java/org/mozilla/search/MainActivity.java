/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.view.View;

import org.mozilla.search.autocomplete.AcceptsSearchQuery;

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

    @Override
    protected void onCreate(Bundle stateBundle) {
        super.onCreate(stateBundle);
        setContentView(R.layout.search_activity_main);
    }

    @Override
    public void onSearch(String s) {
        startPostsearch();
        ((PostSearchFragment) getSupportFragmentManager().findFragmentById(R.id.postsearch))
                .startSearch(s);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // When the app launches, make sure we're in presearch *always*
        startPresearch();
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
}
