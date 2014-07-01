/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;

/**
 * The adapter that is used to populate the autocomplete rows.
 */
class AutoCompleteAdapter extends ArrayAdapter<AutoCompleteModel> {

    private final AcceptsJumpTaps acceptsJumpTaps;

    public AutoCompleteAdapter(Context context, AcceptsJumpTaps acceptsJumpTaps) {
        // Uses '0' for the template id since we are overriding getView
        // and supplying our own view.
        super(context, 0);
        this.acceptsJumpTaps = acceptsJumpTaps;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        AutoCompleteRowView view;

        if (convertView == null) {
            view = new AutoCompleteRowView(getContext());
        } else {
            view = (AutoCompleteRowView) convertView;
        }

        view.setOnJumpListener(acceptsJumpTaps);


        AutoCompleteModel model = getItem(position);

        view.setMainText(model.getMainText());

        return view;
    }
}
