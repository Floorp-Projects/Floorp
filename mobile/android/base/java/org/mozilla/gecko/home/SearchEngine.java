/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.support.annotation.NonNull;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class SearchEngine {
    public static final String LOG_TAG = "GeckoSearchEngine";

    public final String name;             // Never null.
    public final String identifier;       // Can be null.

    private final Bitmap icon;
    private volatile List<String> suggestions = new ArrayList<String>();   // Never null.

    public SearchEngine(final Context context, final GeckoBundle engine) {
        if (engine == null) {
            throw new IllegalArgumentException("Can't instantiate SearchEngine from null bundle.");
        }

        this.name = engine.getString("name");
        if (this.name == null) {
            throw new IllegalArgumentException("Cannot have an unnamed search engine.");
        }

        this.identifier = engine.getString("identifier");

        final String iconURI = engine.getString("iconURI");
        if (iconURI == null) {
            Log.w(LOG_TAG, "iconURI is null for search engine " + this.name);
        }
        final Bitmap tempIcon = BitmapUtils.getBitmapFromDataURI(iconURI);

        this.icon = (tempIcon != null) ? tempIcon : getDefaultFavicon(context);
    }

    private Bitmap getDefaultFavicon(final Context context) {
        return BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_search_icon);
    }

    /**
     * @return a non-null string suitable for use by FHR.
     */
    @NonNull
    public String getEngineIdentifier() {
        if (this.identifier != null) {
            return this.identifier;
        }
        if (this.name != null) {
            return "other-" + this.name;
        }
        return "other";
    }

    public boolean hasSuggestions() {
        return !this.suggestions.isEmpty();
    }

    public int getSuggestionsCount() {
        return this.suggestions.size();
    }

    public Iterable<String> getSuggestions() {
        return this.suggestions;
    }

    public void setSuggestions(List<String> suggestions) {
        if (suggestions == null) {
            this.suggestions = new ArrayList<String>();
            return;
        }
        this.suggestions = suggestions;
    }

    public Bitmap getIcon() {
        return this.icon;
    }
}

