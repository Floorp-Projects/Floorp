/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.app.AlertDialog;
import android.content.Context;
import android.view.LayoutInflater;

import android.view.View;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.homepanel.ActivityStreamConfiguration;

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
        final View panelDialogView = inflater.inflate(R.layout.preference_topsites_panel_dialog, null);

        if (!ActivityStreamConfiguration.isPocketEnabledByLocale(getContext())) {
            final View pocketPreferenceView = panelDialogView.findViewById(R.id.preference_pocket);
            pocketPreferenceView.setVisibility(View.GONE);
        }

        builder.setView(panelDialogView);
    }
}