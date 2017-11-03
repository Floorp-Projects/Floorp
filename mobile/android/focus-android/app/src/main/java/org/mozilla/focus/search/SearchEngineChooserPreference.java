/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.preference.Preference;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.Settings;

import java.util.List;

public class SearchEngineChooserPreference extends Preference implements RadioGroup.OnCheckedChangeListener {
    private List<SearchEngine> searchEngines;

    public SearchEngineChooserPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.preference_search_engine_chooser);
    }

    public SearchEngineChooserPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setLayoutResource(R.layout.preference_search_engine_chooser);
    }

    @Override
    protected View onCreateView(ViewGroup parent) {
        final View layoutView = super.onCreateView(parent);
        final RadioGroup searchEngineGroup = layoutView.findViewById(R.id.search_engine_group);
        searchEngineGroup.setOnCheckedChangeListener(this);
        refreshSearchEngines(searchEngineGroup);
        return layoutView;
    }

    @Override
    public void onCheckedChanged (RadioGroup group, int checkedId) {
        Settings.getInstance(group.getContext()).setDefaultSearchEngine(searchEngines.get(checkedId));
    }

    private void refreshSearchEngines(final RadioGroup engineRadioGroup) {
        final SearchEngineManager sem = SearchEngineManager.getInstance();
        final Context context = engineRadioGroup.getContext();
        final Resources resources = context.getResources();

        searchEngines = sem.getSearchEngines();
        final String defaultSearchEngine = sem.getDefaultSearchEngine(context).getIdentifier();

        engineRadioGroup.removeAllViews();

        final LayoutInflater layoutInflater = LayoutInflater.from(context);
        final RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        for (int i = 0; i < searchEngines.size(); i++) {
            final SearchEngine engine = searchEngines.get(i);
            final RadioButton engineItem = makeRadioButtonFromSearchEngine(engine, layoutInflater, resources);
            engineItem.setId(i);
            if (engine.getIdentifier().equals(defaultSearchEngine)) {
                engineItem.setChecked(true);
            }
            engineRadioGroup.addView(engineItem, layoutParams);
        }
    }

    private static RadioButton makeRadioButtonFromSearchEngine(SearchEngine engine, LayoutInflater layoutInflater, Resources res) {
        final RadioButton radioButton = (RadioButton) layoutInflater.inflate(R.layout.search_engine_radio_button, null);
        radioButton.setText(engine.getName());
        final int iconSize = (int) res.getDimension(R.dimen.preference_icon_drawable_size);
        final BitmapDrawable engineIcon = new BitmapDrawable(res, engine.getIcon());
        engineIcon.setBounds(0, 0, iconSize, iconSize);
        radioButton.setCompoundDrawablesWithIntrinsicBounds(engineIcon, null, null, null);
        return radioButton;
    }
}
