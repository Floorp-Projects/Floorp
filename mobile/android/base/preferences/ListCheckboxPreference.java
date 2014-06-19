/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Checkable;

import org.mozilla.gecko.R;

/**
  * This preference shows a checkbox on its left hand side, but will show a menu when clicked.
  * Its used for preferences like "Clear on Exit" that have a boolean on-off state, but that represent
  * multiple boolean options inside.
  **/
class ListCheckboxPreference extends MultiChoicePreference implements Checkable {
    private static final String LOGTAG = "GeckoListCheckboxPreference";
    private boolean checked;

    public ListCheckboxPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.preference_checkbox);
    }

    @Override
    public boolean isChecked() {
        return checked;
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        View checkboxView = view.findViewById(R.id.checkbox);
        if (checkboxView != null && checkboxView instanceof Checkable) {
            ((Checkable) checkboxView).setChecked(checked);
        }
    }

    @Override
    public void setChecked(boolean checked) {
        boolean changed = checked != this.checked;
        this.checked = checked;
        if (changed) {
            notifyDependencyChange(shouldDisableDependents());
            notifyChanged();
        }
    }

    @Override
    public void toggle() {
        checked = !checked;
    }
}
