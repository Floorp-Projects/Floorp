/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.providers;

import android.graphics.Color;
import android.net.Uri;
import android.util.Log;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;

/**
 * Extend this class to add a new search engine to
 * the search activity.
 */
public class SearchEngine {
    private static final String LOG_TAG = "SearchEngine";

    private static final String URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
    private static final String URLTYPE_SEARCH_HTML  = "text/html";

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

    // Boilerplate bookmarklet-style JS for injecting CSS into the
    // head of a web page. The actual CSS is inserted at `%s`.
    private static final String STYLE_INJECTION_SCRIPT =
            "javascript:(function(){" +
                    "var tag=document.createElement('style');" +
                    "tag.type='text/css';" +
                    "document.getElementsByTagName('head')[0].appendChild(tag);" +
                    "tag.innerText='%s'})();";

    private String identifier;
    private String shortName;
    private String iconURL;

    // TODO: Make something more robust (like EngineURL in nsSearchService.js)
    private Uri resultsUri;
    private Uri suggestUri;

    /**
     *
     * @param in InputStream of open search plugin XML
     */
    public SearchEngine(String identifier, InputStream in) throws IOException, XmlPullParserException {
        this.identifier = identifier;

        final XmlPullParser parser = Xml.newPullParser();
        parser.setInput(in, null);
        parser.nextTag();
        readSearchPlugin(parser);
    }

    private void readSearchPlugin(XmlPullParser parser) throws XmlPullParserException, IOException {
        parser.require(XmlPullParser.START_TAG, null, "SearchPlugin");
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }

            final String tag = parser.getName();
            if (tag.equals("ShortName")) {
                readShortName(parser);
            } else if (tag.equals("Url")) {
                readUrl(parser);
            } else if (tag.equals("Image")) {
                readImage(parser);
            } else {
                skip(parser);
            }
        }
    }

    private void readShortName(XmlPullParser parser) throws IOException, XmlPullParserException {
        parser.require(XmlPullParser.START_TAG, null, "ShortName");
        if (parser.next() == XmlPullParser.TEXT) {
            shortName = parser.getText();
            parser.nextTag();
        }
    }

    private void readUrl(XmlPullParser parser) throws XmlPullParserException, IOException {
        parser.require(XmlPullParser.START_TAG, null, "Url");

        final String type = parser.getAttributeValue(null, "type");
        final String template = parser.getAttributeValue(null, "template");

        Uri uri = Uri.parse(template);

        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }

            final String tag = parser.getName();

            if (tag.equals("Param")) {
                final String name = parser.getAttributeValue(null, "name");
                final String value = parser.getAttributeValue(null, "value");
                uri = uri.buildUpon().appendQueryParameter(name, value).build();
                parser.nextTag();
            // TODO: Support for other tags
            //} else if (tag.equals("MozParam")) {
            } else {
                skip(parser);
            }
        }

        if (type.equals(URLTYPE_SEARCH_HTML)) {
            resultsUri = uri;
        } else if (type.equals(URLTYPE_SUGGEST_JSON)) {
            suggestUri = uri;
        }
    }

    private void readImage(XmlPullParser parser) throws XmlPullParserException, IOException {
        parser.require(XmlPullParser.START_TAG, null, "Image");

        // TODO: Use width and height to get a preferred icon URL.
        //final int width = Integer.parseInt(parser.getAttributeValue(null, "width"));
        //final int height = Integer.parseInt(parser.getAttributeValue(null, "height"));

        if (parser.next() == XmlPullParser.TEXT) {
            iconURL = parser.getText();
            parser.nextTag();
        }
    }

    private void skip(XmlPullParser parser) throws XmlPullParserException, IOException {
        if (parser.getEventType() != XmlPullParser.START_TAG) {
            throw new IllegalStateException();
        }
        int depth = 1;
        while (depth != 0) {
            switch (parser.next()) {
                case XmlPullParser.END_TAG:
                    depth--;
                    break;
                case XmlPullParser.START_TAG:
                    depth++;
                    break;
            }
        }
    }

    /**
     * HACKS! We'll need to replace this with endpoints that return the correct content.
     *
     * Retrieve a JS snippet, in bookmarklet style, that can be used
     * to modify the results page.
     */
    public String getInjectableJs() {
        final String css;

        if (identifier.equals("bing")) {
            css = "#mHeader{display:none}#contentWrapper{margin-top:0}";
        } else if (identifier.equals("google")) {
            css = "#sfcnt,#top_nav{display:none}";
        } else if (identifier.equals("yahoo")) {
            css = "#nav,#header{display:none}";
        } else {
            css = "";
        }

        return String.format(STYLE_INJECTION_SCRIPT, css);
    }

    public String getIdentifier() {
        return identifier;
    }

    public String getName() {
        return shortName;
    }

    public String getIconURL() {
        return iconURL;
    }

    public int getColor() {
        // TOOD: Add brand colors to search plugin XML.
        if (identifier.equals("yahoo")) {
            return 0xFF500095;
        }
        return Color.TRANSPARENT;
    }

    /**
     * Determine whether a particular url belongs to this search engine. If not,
     * the url will be sent to Fennec.
     */
    public boolean isSearchResultsPage(String url) {
        return resultsUri.getAuthority().equalsIgnoreCase(Uri.parse(url).getAuthority());
    }

    /**
     * Create a uri string that can be used to fetch the results page.
     *
     * @param query The user's query. This method will escape and encode the query.
     */
    public String resultsUriForQuery(String query) {
        if (resultsUri == null) {
            Log.e(LOG_TAG, "No results URL for search engine: " + identifier);
            return "";
        }
        final String template = Uri.decode(resultsUri.toString());
        return paramSubstitution(template, Uri.encode(query));
    }

    /**
     * Create a uri string to fetch autocomplete suggestions.
     *
     * @param query The user's query. This method will escape and encode the query.
     */
    public String getSuggestionTemplate(String query) {
        if (suggestUri == null) {
            Log.e(LOG_TAG, "No suggestions template for search engine: " + identifier);
            return "";
        }
        final String template = Uri.decode(suggestUri.toString());
        return paramSubstitution(template, Uri.encode(query));
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
