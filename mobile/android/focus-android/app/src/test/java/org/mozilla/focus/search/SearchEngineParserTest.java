/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.UUID;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(ParameterizedRobolectricTestRunner.class)
public class SearchEngineParserTest {
    @ParameterizedRobolectricTestRunner.Parameters(name = "{1}")
    public static Collection<Object[]> searchPlugins() {
        final Collection<Object[]> searchPlugins = new ArrayList<>();
        collectSearchPlugins(searchPlugins, getBasePath());
        return searchPlugins;
    }

    private String searchPluginPath;
    private String searchEngineIdentifier;

    public SearchEngineParserTest(String searchPluginPath, String searchEngineIdentifier) {
        this.searchPluginPath = searchPluginPath;
        this.searchEngineIdentifier = searchEngineIdentifier;
    }

    @Test
    public void testParser() throws Exception {
        final InputStream stream = new FileInputStream(searchPluginPath);
        final SearchEngine searchEngine = SearchEngineParser.load(searchEngineIdentifier, stream);
        assertEquals(searchEngineIdentifier, searchEngine.getIdentifier());

        assertNotNull(searchEngine.getName());
        assertFalse(TextUtils.isEmpty(searchEngine.getName()));

        assertNotNull(searchEngine.getIcon());

        final String searchTerm = UUID.randomUUID().toString();
        final String searchUrl = searchEngine.buildSearchUrl(searchTerm);

        assertNotNull(searchUrl);
        assertFalse(TextUtils.isEmpty(searchUrl));
        assertTrue(searchUrl.contains(searchTerm));

        stream.close();
    }

    private static void collectSearchPlugins(Collection<Object[]> searchPlugins, File path) {
        if (!path.isDirectory()) {
            throw new AssertionError("Not a directory: " + path.getAbsolutePath());
        }

        final String[] entries = path.list();

        for (String entry : entries) {
            final File file = new File(path, entry);

            if (file.isDirectory()) {
                collectSearchPlugins(searchPlugins, file);
            } else if (entry.endsWith(".xml")) {
                searchPlugins.add(new Object[] { file.getAbsolutePath(), entry.substring(0, entry.length() - 4) });
            }
        }
    }

    private static File getBasePath() {
        final ClassLoader classLoader = SearchEngineParserTest.class.getClassLoader();
        return new File(classLoader.getResource("search").getFile());
    }
}