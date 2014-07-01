/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;


import android.content.Context;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.mozilla.search.R;

/**
 * One row withing the autocomplete suggestion list.
 */
class AutoCompleteRowView extends LinearLayout {

    private TextView textView;
    private AcceptsJumpTaps onJumpListener;

    public AutoCompleteRowView(Context context) {
        super(context);
        init();
    }

    private void init() {
        setOrientation(LinearLayout.HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);

        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.search_auto_complete_row, this, true);

        textView = (TextView) findViewById(R.id.auto_complete_row_text);

        findViewById(R.id.auto_complete_row_jump_button).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (null == onJumpListener) {
                    Log.e("SuggestionRow.onJump", "jump listener is null");
                    return;
                }

                onJumpListener.onJumpTap(textView.getText().toString());
            }
        });
    }

    public void setMainText(String s) {
        textView.setText(s);
    }

    public void setOnJumpListener(AcceptsJumpTaps onJumpListener) {
        this.onJumpListener = onJumpListener;
    }
}
