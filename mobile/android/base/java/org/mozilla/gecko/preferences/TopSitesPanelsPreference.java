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

    TopSitesPanelsPreference(final Context context, final CustomListCategory parentCategory, final boolean isRemovable,
            final boolean isHidden, final int index, final boolean animate) {
        super(context, parentCategory, isRemovable, isHidden, index, animate);
    }

    @Override
    protected void configureDialogBuilder(AlertDialog.Builder builder) {
        final LayoutInflater inflater = LayoutInflater.from(getContext());
        builder.setView(inflater.inflate(R.layout.preference_topsites_panel_dialog, null));
    }
}