/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.preference.PreferenceCategory;
import android.util.AttributeSet;

public abstract class CustomListCategory extends PreferenceCategory {
    protected CustomListPreference mDefaultReference;

    public CustomListCategory(Context context) {
        super(context);
    }

    public CustomListCategory(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public CustomListCategory(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onAttachedToActivity() {
        super.onAttachedToActivity();

        setOrderingAsAdded(true);
    }

    /**
     * Set the default to some available list item. Used if the current default is removed or
     * disabled.
     */
    protected void setFallbackDefault() {
        if (getPreferenceCount() > 0) {
            CustomListPreference aItem = (CustomListPreference) getPreference(0);
            setDefault(aItem);
        }
    }

    /**
     * Removes the given item from the set of available list items.
     * This only updates the UI, so callers are responsible for persisting any state.
     *
     * @param item The given item to remove.
     */
    public void uninstall(CustomListPreference item) {
        removePreference(item);
        if (item == mDefaultReference) {
            // If the default is being deleted, set a new default.
            setFallbackDefault();
        }
    }

    /**
     * Sets the given item as the current default.
     * This only updates the UI, so callers are responsible for persisting any state.
     *
     * @param item The intended new default.
     */
    public void setDefault(CustomListPreference item) {
        if (mDefaultReference != null) {
            mDefaultReference.setIsDefault(false);
        }

        item.setIsDefault(true);
        mDefaultReference = item;
    }
}
