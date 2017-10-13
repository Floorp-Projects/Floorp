/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;

/**
 * Preference for setting the default search engine.
 */
public class SearchEnginePreference extends Preference {
    public SearchEnginePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SearchEnginePreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onAttachedToActivity() {
        setTitle(SearchEngineManager.getInstance().getDefaultSearchEngine(getContext()).getName());
        super.onAttachedToActivity();
    }
}
