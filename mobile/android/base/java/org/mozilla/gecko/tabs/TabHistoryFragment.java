/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.R;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListView;

public class TabHistoryFragment extends Fragment implements OnItemClickListener, OnClickListener {
    private static final String ARG_LIST = "historyPageList";
    private static final String ARG_INDEX = "index";
    private static final String BACK_STACK_ID = "backStateId";

    private List<TabHistoryPage> historyPageList;
    private int toIndex;
    private ListView dialogList;
    private int backStackId = -1;
    private ViewGroup parent;
    private boolean dismissed;

    public TabHistoryFragment() {

    }

    public static TabHistoryFragment newInstance(List<TabHistoryPage> historyPageList, int toIndex) {
        final TabHistoryFragment fragment = new TabHistoryFragment();
        final Bundle args = new Bundle();
        args.putParcelableArrayList(ARG_LIST, (ArrayList<? extends Parcelable>) historyPageList);
        args.putInt(ARG_INDEX, toIndex);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            backStackId = savedInstanceState.getInt(BACK_STACK_ID, -1);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        this.parent = container;
        parent.setVisibility(View.VISIBLE);
        View view = inflater.inflate(R.layout.tab_history_layout, container, false);
        view.setOnClickListener(this);
        dialogList = (ListView) view.findViewById(R.id.tab_history_list);
        dialogList.setDivider(null);
        return view;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        Bundle bundle = getArguments();
        historyPageList = bundle.getParcelableArrayList(ARG_LIST);
        toIndex = bundle.getInt(ARG_INDEX);
        final ArrayAdapter<TabHistoryPage> urlAdapter = new TabHistoryAdapter(getActivity(), historyPageList);
        dialogList.setAdapter(urlAdapter);
        dialogList.setOnItemClickListener(this);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        String index = String.valueOf(toIndex - position);
        GeckoAppShell.notifyObservers("Session:Navigate", index);
        dismiss();
    }

    @Override
    public void onClick(View v) {
        // Since the fragment view fills the entire screen, any clicks outside of the history
        // ListView will end up here.
        dismiss();
    }

    @Override
    public void onPause() {
        super.onPause();
        dismiss();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        dismiss();

        GeckoApplication.watchReference(getActivity(), this);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (backStackId >= 0) {
            outState.putInt(BACK_STACK_ID, backStackId);
        }
    }

    // Function to add this fragment to activity state with containerViewId as parent.
    // This similar in functionality to DialogFragment.show() except that containerId is provided here.
    public void show(final int containerViewId, final FragmentTransaction transaction, final String tag) {
        dismissed = false;
        transaction.add(containerViewId, this, tag);
        transaction.addToBackStack(tag);
        backStackId = transaction.commit();
    }

    // Pop the fragment from backstack if it exists.
    public void dismiss() {
        if (dismissed) {
            return;
        }

        dismissed = true;

        if (backStackId >= 0) {
            getFragmentManager().popBackStackImmediate(backStackId, FragmentManager.POP_BACK_STACK_INCLUSIVE);
            backStackId = -1;
        }

        if (parent != null) {
            parent.setVisibility(View.GONE);
        }
    }

    private static class TabHistoryAdapter extends ArrayAdapter<TabHistoryPage> {
        private final List<TabHistoryPage> pages;
        private final Context context;

        public TabHistoryAdapter(Context context, List<TabHistoryPage> pages) {
            super(context, R.layout.tab_history_item_row, pages);
            this.context = context;
            this.pages = pages;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            TabHistoryItemRow row = (TabHistoryItemRow) convertView;
            if (row == null) {
                row = new TabHistoryItemRow(context, null);
            }

            row.update(pages.get(position), position == 0, position == pages.size() - 1);
            return row;
        }
    }
}
