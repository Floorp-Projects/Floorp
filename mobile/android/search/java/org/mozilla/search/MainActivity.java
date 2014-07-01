/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;

import org.mozilla.search.autocomplete.AcceptsSearchQuery;
import org.mozilla.search.autocomplete.AutoCompleteFragment;
import org.mozilla.search.stream.CardStreamFragment;


/**
 * The main entrance for the Android search intent.
 * <p/>
 * State management is delegated to child fragments. Fragments communicate
 * with each other by passing messages through this activity. The only message passing right
 * now, the only message passing occurs when a user wants to submit a search query. That
 * passes through the onSearch method here.
 */
public class MainActivity extends FragmentActivity implements AcceptsSearchQuery,
        FragmentManager.OnBackStackChangedListener {

    private DetailActivity detailActivity;

    @Override
    protected void onCreate(Bundle stateBundle) {
        super.onCreate(stateBundle);

        // Sets the content view for the Activity
        setContentView(R.layout.search_activity_main);

        // Gets an instance of the support library FragmentManager
        FragmentManager localFragmentManager = getSupportFragmentManager();

        // If the incoming state of the Activity is null, sets the initial view to be thumbnails
        if (null == stateBundle) {

            // Starts a Fragment transaction to track the stack
            FragmentTransaction localFragmentTransaction = localFragmentManager.beginTransaction();

            localFragmentTransaction.add(R.id.header_fragments, new AutoCompleteFragment(),
                    Constants.AUTO_COMPLETE_FRAGMENT);

            localFragmentTransaction.add(R.id.presearch_fragments, new CardStreamFragment(),
                    Constants.CARD_STREAM_FRAGMENT);

            // Commits this transaction to display the Fragment
            localFragmentTransaction.commit();

            // The incoming state of the Activity isn't null.
        }
    }

    @Override
    protected void onStart() {
        super.onStart();


        if (null == detailActivity) {
            detailActivity = new DetailActivity();
        }

        if (null == getSupportFragmentManager().findFragmentByTag(Constants.GECKO_VIEW_FRAGMENT)) {
            FragmentTransaction txn = getSupportFragmentManager().beginTransaction();
            txn.add(R.id.gecko_fragments, detailActivity, Constants.GECKO_VIEW_FRAGMENT);
            txn.hide(detailActivity);

            txn.commit();
        }
    }

    @Override
    public void onSearch(String s) {
        FragmentManager localFragmentManager = getSupportFragmentManager();
        FragmentTransaction localFragmentTransaction = localFragmentManager.beginTransaction();

        localFragmentTransaction
                .hide(localFragmentManager.findFragmentByTag(Constants.CARD_STREAM_FRAGMENT))
                .addToBackStack(null);

        localFragmentTransaction
                .show(localFragmentManager.findFragmentByTag(Constants.GECKO_VIEW_FRAGMENT))
                .addToBackStack(null);

        localFragmentTransaction.commit();


        ((DetailActivity) getSupportFragmentManager()
                .findFragmentByTag(Constants.GECKO_VIEW_FRAGMENT))
                .setUrl("https://search.yahoo.com/search?p=" + Uri.encode(s));
    }

    @Override
    public void onBackStackChanged() {

    }
}
