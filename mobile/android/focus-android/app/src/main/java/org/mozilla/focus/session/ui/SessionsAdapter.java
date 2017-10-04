/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui;

import android.arch.lifecycle.Observer;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import org.mozilla.focus.session.Session;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Adapter implementation to show a list of active browsing sessions and an "erase" button at the end.
 */
public class SessionsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> implements Observer<List<Session>> {
    private final SessionsSheetFragment fragment;
    private List<Session> sessions;

    /* package */ SessionsAdapter(SessionsSheetFragment fragment) {
        this.fragment = fragment;
        this.sessions = Collections.emptyList();
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        switch (viewType) {
            case EraseViewHolder.LAYOUT_ID:
                return new EraseViewHolder(
                        fragment,
                        inflater.inflate(EraseViewHolder.LAYOUT_ID, parent, false));
            case SessionViewHolder.LAYOUT_ID:
                return new SessionViewHolder(
                        fragment,
                        (TextView) inflater.inflate(SessionViewHolder.LAYOUT_ID, parent, false));
            default:
                throw new IllegalStateException("Unknown viewType");
        }
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        switch (holder.getItemViewType()) {
            case EraseViewHolder.LAYOUT_ID:
                // Nothing to do here.
                break;
            case SessionViewHolder.LAYOUT_ID:
                ((SessionViewHolder) holder).bind(sessions.get(position));
                break;
            default:
                throw new IllegalStateException("Unknown viewType");
        }
    }

    @Override
    public int getItemViewType(int position) {
        if (isErasePosition(position)) {
            return EraseViewHolder.LAYOUT_ID;
        } else {
            return SessionViewHolder.LAYOUT_ID;
        }
    }

    private boolean isErasePosition(int position) {
        return position == sessions.size();
    }

    @Override
    public int getItemCount() {
        return sessions.size() + 1;
    }

    @Override
    public void onChanged(List<Session> sessions) {
        this.sessions = new ArrayList<>(sessions);
        notifyDataSetChanged();
    }
}
