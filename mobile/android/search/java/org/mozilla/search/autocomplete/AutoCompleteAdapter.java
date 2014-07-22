/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.mozilla.search.R;

import java.util.List;

/**
 * The adapter that is used to populate the autocomplete rows.
 */
class AutoCompleteAdapter extends ArrayAdapter<String> {

    private final AcceptsJumpTaps acceptsJumpTaps;

    private final LayoutInflater inflater;

    public AutoCompleteAdapter(Context context, AcceptsJumpTaps acceptsJumpTaps) {
        // Uses '0' for the template id since we are overriding getView
        // and supplying our own view.
        super(context, 0);
        this.acceptsJumpTaps = acceptsJumpTaps;

        // Disable notifying on change. We will notify ourselves in update.
        setNotifyOnChange(false);

        inflater = LayoutInflater.from(context);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            convertView = inflater.inflate(R.layout.search_auto_complete_row, null);
        }

        final String text = getItem(position);

        final TextView textView = (TextView) convertView.findViewById(R.id.auto_complete_row_text);
        textView.setText(text);

        final View jumpButton = convertView.findViewById(R.id.auto_complete_row_jump_button);
        jumpButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                acceptsJumpTaps.onJumpTap(text);
            }
        });

        return convertView;
    }

    /**
     * Updates adapter content with new list of search suggestions.
     *
     * @param suggestions List of search suggestions.
     */
    public void update(List<String> suggestions) {
        clear();
        if (suggestions != null) {
            for (String s : suggestions) {
                add(s);
            }
        }
        notifyDataSetChanged();
    }
}
