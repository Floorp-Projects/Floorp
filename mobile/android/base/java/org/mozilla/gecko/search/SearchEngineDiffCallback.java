/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.search;

import android.support.annotation.Nullable;
import android.support.v7.util.DiffUtil;

import org.mozilla.gecko.preferences.SearchEnginePreference;

import java.util.List;
/**
 * SearchEngineDiffCallback.java - class used by diffutils to maintain List differential array.
 * @author  Brad Arant
 * @version
 * @see SearchPreferenceCategory
 */
public class SearchEngineDiffCallback extends DiffUtil.Callback {

    private List<SearchEnginePreference> oldList;
    private List<SearchEnginePreference> newList;

    public SearchEngineDiffCallback(List<SearchEnginePreference> oldList, List<SearchEnginePreference> newList) {
        this.oldList = oldList;
        this.newList = newList;
    }

    @Override
    public int getOldListSize() {
        return oldList.size();
    }

    @Override
    public int getNewListSize() {
        return newList.size();
    }

    @Override
    public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
        return oldList.get(oldItemPosition).getIdentifier().equals(newList.get(newItemPosition).getIdentifier());
    }

    @Override
    public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
        if (oldList.get(oldItemPosition).getTitle() != newList.get(newItemPosition).getTitle()) {
            return false;
        }
        return oldList.get(oldItemPosition).getIdentifier().equals(newList.get(newItemPosition).getIdentifier());
    }

    @Nullable
    @Override
    public Object getChangePayload(int oldItemPosition, int newItemPosition) {
        return super.getChangePayload(oldItemPosition, newItemPosition);
    }
}
