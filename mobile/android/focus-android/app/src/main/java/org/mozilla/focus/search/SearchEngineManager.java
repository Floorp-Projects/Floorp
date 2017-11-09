/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.support.annotation.WorkerThread;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.focus.ext.AssetManagerKt;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.shortcut.IconGenerator;
import org.mozilla.focus.utils.BitmapUtils;
import org.mozilla.focus.utils.Settings;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.xmlpull.v1.XmlPullParserException;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Set;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import edu.umd.cs.findbugs.annotations.SuppressFBWarnings;

public class SearchEngineManager extends BroadcastReceiver {
    private static final String LOG_TAG = SearchEngineManager.class.getSimpleName();

    public static final String PREF_FILE_SEARCH_ENGINES = "custom-search-engines";
    public static final String PREF_KEY_CUSTOM_SEARCH_ENGINES = "pref_custom_search_engines";
    public static final String PREF_KEY_HIDDEN_DEFAULT_ENGINES = "hidden_default_engines";
    private static final String PREF_KEY_CUSTOM_SEARCH_VERSION = "pref_custom_search_version";
    private static final int CUSTOM_SEARCH_VERSION = 1;

    private static SearchEngineManager instance = new SearchEngineManager();

    private List<SearchEngine> searchEngines;

    /**
     * A flag indicating that data has been loaded, or is loading. This lets us detect if data
     * has been requested without a preceeding init().
     */
    private boolean loadHasBeenTriggered = false;

    public static SearchEngineManager getInstance() {
        return instance;
    }

    private SearchEngineManager() {}

    public static boolean addSearchEngine(SharedPreferences sharedPreferences, Context context, String engineName, String searchQuery) {
        final Bitmap iconBitmap = IconGenerator.generateLauncherIcon(context, searchQuery);
        final String searchEngineXml = buildSearchEngineXML(engineName, searchQuery, iconBitmap);
        if (searchEngineXml == null) {
            return false;
        }
        final Set<String> existingEngines = sharedPreferences.getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, Collections.<String>emptySet());
        final Set<String> newEngines = new LinkedHashSet<>();
        newEngines.addAll(existingEngines);
        newEngines.add(engineName);

        sharedPreferences.edit().putInt(PREF_KEY_CUSTOM_SEARCH_VERSION, CUSTOM_SEARCH_VERSION)
                .putStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, newEngines)
                .putString(engineName, searchEngineXml)
                .apply();

        return true;
    }

    public static String buildSearchEngineXML(String engineName, String searchQuery, Bitmap iconBitmap) {
        Document document = null;
        try {
            document = DocumentBuilderFactory.newInstance().newDocumentBuilder().newDocument();
            final Element rootElement = document.createElement("OpenSearchDescription");
            rootElement.setAttribute("xmlns", "http://a9.com/-/spec/opensearch/1.1/");
            document.appendChild(rootElement);

            final Element shortNameElement = document.createElement("ShortName");
            shortNameElement.setTextContent(engineName);
            rootElement.appendChild(shortNameElement);

            final Element imageElement = document.createElement("Image");
            imageElement.setAttribute("width", "16");
            imageElement.setAttribute("height", "16");
            imageElement.setTextContent(BitmapUtils.getBase64EncodedDataUriFromBitmap(iconBitmap));
            rootElement.appendChild(imageElement);

            final Element descriptionElement = document.createElement("Description");
            descriptionElement.setTextContent(engineName);
            rootElement.appendChild(descriptionElement);

            final Element urlElement = document.createElement("Url");
            urlElement.setAttribute("type", "text/html");
            // Simple implementation that assumes "%s" terminator from UrlUtils.isValidSearchEngineQueryUrl
            final String templateSearchString = searchQuery.substring(0, searchQuery.length() - 2) + "{searchTerms}";
            urlElement.setAttribute("template", templateSearchString);
            rootElement.appendChild(urlElement);

        } catch (ParserConfigurationException e) {
            Log.e(LOG_TAG, "Couldn't create new Document for building search engine XML", e);
            return null;
        }
        return XMLtoString(document);
    }

    private static String XMLtoString(Document doc) {
        if (doc == null) {
            return null;
        }

        final Writer writer = new StringWriter();
        try {
            final Transformer tf = TransformerFactory.newInstance().newTransformer();
            tf.setOutputProperty(OutputKeys.ENCODING, "UTF-8");
            tf.transform(new DOMSource(doc), new StreamResult(writer));
        } catch (TransformerConfigurationException e) {
            return null;
        } catch (TransformerException e) {
            return null;
        }
        return writer.toString();
    }

    public void init(Context context) {
        context.registerReceiver(this, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));

        loadSearchEngines(context);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!Intent.ACTION_LOCALE_CHANGED.equals(intent.getAction())) {
            // This is not the broadcast you are looking for.
            return;
        }

        loadSearchEngines(context.getApplicationContext());
    }

    public void loadSearchEngines(final Context context) {
        new Thread("SearchEngines-Load") {
            @Override
            public void run() {
                invalidateSearchEngines();
                loadFromDisk(context);
            }
        }.start();
    }

    @WorkerThread
    private synchronized void loadFromDisk(Context context) {
        loadHasBeenTriggered = true;
        final AssetManager assetManager = context.getAssets();
        final Locale locale = Locale.getDefault();
        final List<SearchEngine> searchEngines = new ArrayList<>();

        try {
            final JSONArray engineNames = loadSearchEngineListForLocale(context);

            final String localePath = "search/" + Locales.getLanguageTag(locale);
            final String languagePath = "search/" + Locales.getLanguage(locale);
            final String defaultPath = "search/default";

            final List<String> localeEngines = Arrays.asList(assetManager.list(localePath));
            final List<String> languageEngines = Arrays.asList(assetManager.list(languagePath));
            final List<String> defaultEngines = Arrays.asList(assetManager.list(defaultPath));

           final SharedPreferences sharedPreferences = context.getSharedPreferences(
                   SearchEngineManager.PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE);
            final Set<String> hiddenEngines = sharedPreferences.getStringSet(PREF_KEY_HIDDEN_DEFAULT_ENGINES, Collections.<String>emptySet());

            for (int i = 0; i < engineNames.length(); i++) {
                final String engineName = engineNames.getString(i);
                // Engine names are reused as engine ids, so they are the same.
                if (hiddenEngines.contains(engineName)) {
                    continue;
                }
                final String fileName = engineName + ".xml";

                if (localeEngines.contains(fileName)) {
                    searchEngines.add(SearchEngineParser.load(assetManager, engineName, localePath + "/" + fileName));
                } else if (languageEngines.contains(fileName)) {
                    searchEngines.add(SearchEngineParser.load(assetManager, engineName, languagePath + "/" + fileName));
                } else if (defaultEngines.contains(fileName)) {
                    searchEngines.add(SearchEngineParser.load(assetManager, engineName, defaultPath + "/" + fileName));
                } else {
                    Log.e(LOG_TAG, "Couldn't find configuration for engine: " + engineName);
                }
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "IOException while loading search engines", e);
        } catch (JSONException e) {
            throw new AssertionError("Reading search engine failed: ", e);
        } finally {
            searchEngines.addAll(loadCustomSearchEngines(context));
            this.searchEngines = searchEngines;

            notifyAll();
        }
    }

    private List<SearchEngine> loadCustomSearchEngines(Context context) {
        final List<SearchEngine> searchEngines = new LinkedList<>();
        final SharedPreferences prefs = context.getSharedPreferences(PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE);
        final Set<String> engines = prefs.getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, Collections.<String>emptySet());
        try {
            for (String engine : engines) {
                final InputStream engineInputStream = new ByteArrayInputStream(prefs.getString(engine, "").getBytes(StandardCharsets.UTF_8));
                searchEngines.add(SearchEngineParser.load(engine, engineInputStream));
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "IOException while loading custom search engines", e);
        } catch (XmlPullParserException e) {
            Log.e(LOG_TAG, "Couldn't load custom search engines", e);
        }
        return searchEngines;
    }

    private JSONArray loadSearchEngineListForLocale(Context context) throws IOException {
        try {
            final Locale locale = Locale.getDefault();

            final JSONObject configuration = AssetManagerKt.readJSONObject(context.getAssets(), "search/search_configuration.json");

            // Try to find a configuration for the language tag first (de-DE)
            final String languageTag = Locales.getLanguageTag(locale);
            if (configuration.has(languageTag)) {
                return configuration.getJSONArray(languageTag);
            }

            // Try to find a configuration for just the language (de)
            final String language = Locales.getLanguage(locale);
            if (configuration.has(language)) {
                return configuration.getJSONArray(language);
            }

            // No configuration for the current locale found. Let's use the default configuration.
            return configuration.getJSONArray("default");
        } catch (JSONException e) {
            // Assertion error because this shouldn't happen: We check whether a key exists before
            // reading it. An error here would mean the JSON file is corrupt.
            throw new AssertionError("Reading search configuration failed", e);
        }
    }

    public synchronized List<SearchEngine> getSearchEngines() {
        awaitLoadingSearchEnginesLocked();

        return searchEngines;
    }

    public synchronized SearchEngine getDefaultSearchEngine(Context context) {
        awaitLoadingSearchEnginesLocked();

        final String defaultSearch = Settings.getInstance(context).getDefaultSearchEngineName();
        if (defaultSearch != null) {
            for (SearchEngine searchEngine : searchEngines) {
                if (defaultSearch.equals(searchEngine.getName())) {
                    return searchEngine;
                }
            }
        }

        return searchEngines.get(0);
    }

    // Our (searchEngines == null) check is deemed to be an unsynchronised access. Similarly loadHasBeenTriggered
    // also doesn't need synchronisation:
    @SuppressFBWarnings(value = "IS2_INCONSISTENT_SYNC", justification = "Variable is not being accessed, it is merely being tested for existence")
    public void awaitLoadingSearchEnginesLocked() {
        if (!loadHasBeenTriggered) {
            throw new IllegalStateException("Attempting to retrieve search engines without a corresponding init()");
        }

        while (searchEngines == null) {
            try {
                wait();
            } catch (InterruptedException ignored) {
                // Ignore
            }
        }
    }

    public static void removeSearchEngines(Set<String> engineIdsToRemove, SharedPreferences sharedPreferences) {
        // Check custom engines first.
        final Set<String> customEngines = sharedPreferences.getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, Collections.<String>emptySet());
        final Set<String> remainingCustomEngines = new LinkedHashSet<>();
        final SharedPreferences.Editor enginesEditor = sharedPreferences.edit();

        for (String engineId : customEngines) {
            if (engineIdsToRemove.contains(engineId)) {
                enginesEditor.remove(engineId);
                // Handled engine removal.
                engineIdsToRemove.remove(engineId);
            } else {
                remainingCustomEngines.add(engineId);
            }
        }
        enginesEditor.putStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, remainingCustomEngines);

        // Everything else must be a default engine.
        final Set<String> hiddenDefaultEngines = sharedPreferences.getStringSet(PREF_KEY_HIDDEN_DEFAULT_ENGINES, Collections.<String>emptySet());
        engineIdsToRemove.addAll(hiddenDefaultEngines);
        enginesEditor.putStringSet(PREF_KEY_HIDDEN_DEFAULT_ENGINES, engineIdsToRemove);

        enginesEditor.apply();
    }

    public static synchronized void restoreDefaultSearchEngines(SharedPreferences sharedPreferences) {
        sharedPreferences.edit().remove(PREF_KEY_HIDDEN_DEFAULT_ENGINES).commit();
    }

    private void invalidateSearchEngines() {
        searchEngines = null;
    }
}
