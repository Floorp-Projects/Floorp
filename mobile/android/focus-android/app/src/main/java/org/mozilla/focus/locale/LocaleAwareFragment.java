/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


package org.mozilla.focus.locale;

import android.support.v4.app.Fragment;

import java.util.Locale;

public abstract class LocaleAwareFragment extends Fragment {
    private Locale cachedLocale = null;

    /**
     * Is called whenever the application locale has changed. Your fragment must either update
     * all localised Strings, or replace itself with an updated version.
     */
    public abstract void applyLocale();

    @Override
    public void onResume() {
        super.onResume();
        
        LocaleManager.getInstance()
                .correctLocale(getContext(), getResources(), getResources().getConfiguration());

        if (cachedLocale == null) {
            cachedLocale = Locale.getDefault();
        } else {
            Locale newLocale = LocaleManager.getInstance().getCurrentLocale(getActivity().getApplicationContext());

            if (newLocale == null) {
                // Using system locale:
                newLocale = Locale.getDefault();
            }
            if (!newLocale.equals(cachedLocale)) {
                cachedLocale = newLocale;
                applyLocale();
            }
        }
    }
}
