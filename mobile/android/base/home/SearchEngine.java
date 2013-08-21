/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.graphics.Bitmap;

import java.util.ArrayList;

class SearchEngine {
    public String name;
    public String identifier;
    public Bitmap icon;
    public ArrayList<String> suggestions;

    public SearchEngine(String name, String identifier) {
        this(name, identifier, null);
    }

    public SearchEngine(String name, String identifier, Bitmap icon) {
        this.name = name;
        this.identifier = identifier;
        this.icon = icon;
        this.suggestions = new ArrayList<String>();
    }
}

