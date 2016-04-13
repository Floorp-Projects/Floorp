/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.os.Bundle;

import java.util.EnumSet;
import java.util.Locale;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarHelper;

import android.support.design.widget.Snackbar;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * Fragment that used to display reading list contents in a ListView, and now directs
 * users to Bookmarks to view their former reading-list content.
 */
public class ReadingListPanel extends HomeFragment {

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.readinglistpanel_gone_fragment, container, false);

        // We could update the ID names - however this panel is only intended to be live for one
        // release, hence there's little utility in optimising this code.
        root.findViewById(R.id.welcome_account).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean bookmarksEnabled = GeckoSharedPrefs.forProfile(getContext()).getBoolean(HomeConfig.PREF_KEY_BOOKMARKS_PANEL_ENABLED, true);

                if (bookmarksEnabled) {
                    mUrlOpenListener.onUrlOpen("about:home?panel=" + HomeConfig.getIdForBuiltinPanelType(HomeConfig.PanelType.BOOKMARKS),
                            EnumSet.noneOf(HomePager.OnUrlOpenListener.Flags.class));
                } else {
                    SnackbarHelper.showSnackbar(getActivity(),
                            getResources().getString(R.string.reading_list_migration_bookmarks_hidden),
                            Snackbar.LENGTH_LONG);
                }
            }
        });

        root.findViewById(R.id.welcome_browse).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final String link = getString(R.string.migrated_reading_list_url,
                        AppConstants.MOZ_APP_VERSION,
                        AppConstants.OS_TARGET,
                        Locales.getLanguageTag(Locale.getDefault()));

                mUrlOpenListener.onUrlOpen(link,
                        EnumSet.noneOf(HomePager.OnUrlOpenListener.Flags.class));
            }
        });

        return root;
    }

    @Override
    protected void load() {
        // Must be overriden, but we're not doing any loading hence no real implementation...
    }
}
