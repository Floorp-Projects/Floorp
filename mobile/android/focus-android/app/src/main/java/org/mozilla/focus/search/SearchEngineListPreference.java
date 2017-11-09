/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.preference.Preference;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.RadioGroup;

import org.mozilla.focus.R;

import java.util.List;

public abstract class SearchEngineListPreference extends Preference {
    protected List<SearchEngine> searchEngines;
    protected RadioGroup searchEngineGroup;

    public SearchEngineListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.preference_search_engine_chooser);
    }

    public SearchEngineListPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setLayoutResource(R.layout.preference_search_engine_chooser);
    }

    @Override
    protected View onCreateView(ViewGroup parent) {
        final View layoutView = super.onCreateView(parent);
        searchEngineGroup = layoutView.findViewById(R.id.search_engine_group);
        refreshSearchEngines();
        return layoutView;
    }

    protected abstract int getItemResId();
    protected abstract void updateDefaultItem(CompoundButton defaultButton);

    public void refreshSearchEngines() {
        if (searchEngineGroup == null) {
            // There is no search engine group yet.
            return;
        }
        final SearchEngineManager sem = SearchEngineManager.getInstance();
        final Context context = searchEngineGroup.getContext();
        final Resources resources = context.getResources();

        searchEngines = sem.getSearchEngines();
        final String defaultSearchEngine = sem.getDefaultSearchEngine(context).getIdentifier();

        searchEngineGroup.removeAllViews();

        final LayoutInflater layoutInflater = LayoutInflater.from(context);
        final RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        for (int i = 0; i < searchEngines.size(); i++) {
            final SearchEngine engine = searchEngines.get(i);
            final CompoundButton engineItem = makeButtonFromSearchEngine(engine, layoutInflater, resources);
            engineItem.setId(i);
            if (engine.getIdentifier().equals(defaultSearchEngine)) {
                updateDefaultItem(engineItem);
            }
            searchEngineGroup.addView(engineItem, layoutParams);
        }
    }

    private CompoundButton makeButtonFromSearchEngine(SearchEngine engine, LayoutInflater layoutInflater, Resources res) {
        final CompoundButton buttonItem = (CompoundButton) layoutInflater.inflate(getItemResId(), null);
        buttonItem.setText(engine.getName());
        final int iconSize = (int) res.getDimension(R.dimen.preference_icon_drawable_size);
        final BitmapDrawable engineIcon = new BitmapDrawable(res, engine.getIcon());
        engineIcon.setBounds(0, 0, iconSize, iconSize);
        final Drawable[] drawables = buttonItem.getCompoundDrawables();
        buttonItem.setCompoundDrawables(engineIcon, null, drawables[2], null);
        return buttonItem;
    }
}
