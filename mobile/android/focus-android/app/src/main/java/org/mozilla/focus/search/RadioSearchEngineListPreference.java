/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.RadioGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.Settings;

public class RadioSearchEngineListPreference extends SearchEngineListPreference implements RadioGroup.OnCheckedChangeListener {

    public RadioSearchEngineListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public RadioSearchEngineListPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected View onCreateView(ViewGroup parent) {
        final View view = super.onCreateView(parent);
        searchEngineGroup.setOnCheckedChangeListener(this);
        return view;
    }

    @Override
    protected int getItemResId() {
        return R.layout.search_engine_radio_button;
    }

    @Override
    protected void updateDefaultItem(CompoundButton defaultButton) {
        defaultButton.setChecked(true);
    }


    @Override
    public void onCheckedChanged (RadioGroup group, int checkedId) {
        Settings.getInstance(group.getContext()).setDefaultSearchEngine(searchEngines.get(checkedId));
    }
}
