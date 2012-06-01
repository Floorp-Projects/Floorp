/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;

public class FindInPageBar extends RelativeLayout implements TextWatcher, View.OnClickListener {
    private static final String LOGTAG = "GeckoFindInPagePopup";

    private final Context mContext;
    private EditText mFindText;
    private boolean mInflated = false;

    public FindInPageBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        setFocusable(true);
    }

    public void inflateContent() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        View content = inflater.inflate(R.layout.find_in_page_content, this);

        content.findViewById(R.id.find_prev).setOnClickListener(this);
        content.findViewById(R.id.find_next).setOnClickListener(this);
        content.findViewById(R.id.find_close).setOnClickListener(this);

        mFindText = (EditText) content.findViewById(R.id.find_text);
        mFindText.addTextChangedListener(this);

        mInflated = true;
    }

    public void show() {
        if (!mInflated)
            inflateContent();

        setVisibility(VISIBLE);        
        mFindText.requestFocus();
    }

    public void hide() {
        setVisibility(GONE);        
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FindInPage:Closed", null));
    }

    // TextWatcher implementation

    public void afterTextChanged(Editable s) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FindInPage:Find", s.toString()));
    }

    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        // ignore
    }

    public void onTextChanged(CharSequence s, int start, int before, int count) {
        // ignore
    }

    // View.OnClickListener implementation

    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.find_prev:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FindInPage:Prev", mFindText.getText().toString()));
                break;
            case R.id.find_next:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FindInPage:Next", mFindText.getText().toString()));
                break;
            case R.id.find_close:
                hide();
                break;
        }
    }
}
