/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.view.View;
import android.widget.ArrayAdapter;

import org.mozilla.search.stream.PreloadAgent;


/**
 * This fragment is responsible for managing the card stream. Right now
 * we only use this during pre-search, but we could also use it
 * during post-search at some point.
 */
public class PreSearchFragment extends ListFragment {

    private ArrayAdapter<PreloadAgent.TmpItem> adapter;

    /**
     * Mandatory empty constructor for the fragment manager to instantiate the
     * fragment (e.g. upon screen orientation changes).
     */
    public PreSearchFragment() {
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        getListView().setDivider(null);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        View headerView = getLayoutInflater(savedInstanceState)
                .inflate(R.layout.search_stream_header, getListView(), false);
        getListView().addHeaderView(headerView, null, false);
        if (null == adapter) {
            adapter = new ArrayAdapter<PreloadAgent.TmpItem>(getActivity(), R.layout.search_card,
                    R.id.card_title, PreloadAgent.ITEMS) {
                /**
                 * Return false here disables the ListView from highlighting the click events
                 * for each of the items. Each card should handle its own click events.
                 */
                @Override
                public boolean isEnabled(int position) {
                    return false;
                }
            };
        }

        setListAdapter(adapter);
    }


}
