/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.app.AlertDialog;
import android.content.Context;
import android.view.LayoutInflater;

import org.mozilla.gecko.R;

/**
 * Custom preference that also adds additional options to the dialog of preferences for Top Sites settings.
 */
public class TopSitesPanelsPreference extends PanelsPreference {

    TopSitesPanelsPreference(Context context, CustomListCategory parentCategory, boolean isRemovable, int index, boolean animate) {
        super(context, parentCategory, isRemovable, index, animate);
    }

    @Override
    protected void configureDialogBuilder(AlertDialog.Builder builder) {
        final LayoutInflater inflater = LayoutInflater.from(getContext());
        builder.setView(R.layout.preference_topsites_panel_dialog);
        builder.setView(inflater.inflate(R.layout.preference_topsites_panel_dialog, null));
    }
}