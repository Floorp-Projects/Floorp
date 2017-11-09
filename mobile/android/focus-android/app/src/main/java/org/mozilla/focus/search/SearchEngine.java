/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.graphics.Bitmap;
import android.net.Uri;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import edu.umd.cs.findbugs.annotations.SuppressFBWarnings;

public class SearchEngine {

    // Parameters copied from nsSearchService.js
    private static final String MOZ_PARAM_LOCALE = "\\{moz:locale\\}";
    private static final String MOZ_PARAM_DIST_ID = "\\{moz:distributionID\\}";
    private static final String MOZ_PARAM_OFFICIAL = "\\{moz:official\\}";

    // Supported OpenSearch parameters
    // See http://opensearch.a9.com/spec/1.1/querysyntax/#core
    private static final String OS_PARAM_USER_DEFINED = "\\{searchTerms\\??\\}";
    private static final String OS_PARAM_INPUT_ENCODING = "\\{inputEncoding\\??\\}";
    private static final String OS_PARAM_LANGUAGE = "\\{language\\??\\}";
    private static final String OS_PARAM_OUTPUT_ENCODING = "\\{outputEncoding\\??\\}";
    private static final String OS_PARAM_OPTIONAL = "\\{(?:\\w+:)?\\w+\\?\\}";

    private final String identifier;
    /* package */ String name;
    /* package */ Bitmap icon;
    /* package */ List<Uri> resultsUris;
    // We don't currently support search suggestions, however that's something that we might
    // need to support in future, moreover this is already stored in the input files that we're merely
    // moving into memory - hence we probably want to keep this field for now:
    @SuppressFBWarnings(value = "URF_UNREAD_FIELD", justification = "Needed for future versions, reflects on-disk format")
    /* package */ Uri suggestUri;

    /* package */ SearchEngine(String identifier) {
        this.identifier = identifier;
        this.resultsUris = new ArrayList<>();
    }

    public String getName() {
        return name;
    }

    public String getIdentifier() {
        return identifier;
    }

    public Bitmap getIcon() {
        return icon;
    }

    public String buildSearchUrl(final String searchTerm) {
        if (resultsUris.isEmpty()) {
            return searchTerm;
        }

        // The parse should have put the best URL for this device at the beginning of the list.
        final Uri searchUri = resultsUris.get(0);

        final String template = Uri.decode(searchUri.toString());
        return paramSubstitution(template, Uri.encode(searchTerm));
    }

    public String getBaseSearchUrl() {
        if (!resultsUris.isEmpty()) {
            return resultsUris.get(0).toString();
        } else {
            return null;
        }
    }

    /**
     * Formats template string with proper parameters. Modeled after
     * ParamSubstitution in nsSearchService.js
     *
     * @param template
     * @param query
     * @return
     */
    private String paramSubstitution(String template, String query) {
        final String locale = Locale.getDefault().toString();

        template = template.replaceAll(MOZ_PARAM_LOCALE, locale);
        template = template.replaceAll(MOZ_PARAM_DIST_ID, "");
        template = template.replaceAll(MOZ_PARAM_OFFICIAL, "unofficial");

        template = template.replaceAll(OS_PARAM_USER_DEFINED, query);
        template = template.replaceAll(OS_PARAM_INPUT_ENCODING, "UTF-8");

        template = template.replaceAll(OS_PARAM_LANGUAGE, locale);
        template = template.replaceAll(OS_PARAM_OUTPUT_ENCODING, "UTF-8");

        // Replace any optional parameters
        template = template.replaceAll(OS_PARAM_OPTIONAL, "");

        return template;
    }

}
