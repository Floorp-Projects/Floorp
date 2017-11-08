/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.content.Context;
import android.util.Pair;
import android.util.SparseArray;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.robolectric.RuntimeEnvironment;

import java.util.EnumSet;
import java.util.HashSet;
import java.util.Set;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TestHomeConfigPrefsBackendMigration {

    // Each Pair consists of a list of panels that exist going into a given migration, and a list containing
    // the expected default output panel corresponding to each given input panel in the list of existing panels.
    // E.g. if a given N->N+1 migration starts with panels Foo and Bar, and removes Bar, the two lists would
    // be {Foo, Bar} and {Foo, Foo}.
    // Note: the index where each pair is inserted corresponds to the HomeConfig version before the migration.
    // The final item in this list denotes the current HomeCOnfig version, and therefore only needs to contain
    // the list of panel types that are expected by default (but no list for after the non-existent migration).
    final SparseArray<Pair<PanelType[], PanelType[]>> migrationConstellations = new SparseArray<>();
    {
        // 6->7: the recent tabs panel was merged into the combined history panel
        migrationConstellations.put(6, new Pair<>(
                /* Panels that are expected to exist before this migration happens */
                new PanelType[] {
                        PanelType.TOP_SITES,
                        PanelType.BOOKMARKS,
                        PanelType.COMBINED_HISTORY,
                        PanelType.DEPRECATED_RECENT_TABS
                },
                /* The expected default panel that is expected after the migration */
                new PanelType[] {
                        PanelType.TOP_SITES, /* TOP_SITES remains the default if it was previously the default */
                        PanelType.BOOKMARKS, /* same as TOP_SITES */
                        PanelType.COMBINED_HISTORY, /* same as TOP_SITES */
                        PanelType.COMBINED_HISTORY /* DEPRECATED_RECENT_TABS is replaced by COMBINED_HISTORY during this migration and is therefore the new default */
                }
        ));

        // 7->8: no changes, this was a fixup migration since 6->7 was previously botched
        migrationConstellations.put(7, new Pair<>(
                new PanelType[] {
                        PanelType.TOP_SITES,
                        PanelType.BOOKMARKS,
                        PanelType.COMBINED_HISTORY,
                },
                new PanelType[] {
                        PanelType.TOP_SITES,
                        PanelType.BOOKMARKS,
                        PanelType.COMBINED_HISTORY,
                }
        ));

        migrationConstellations.put(8, new Pair<>(
                new PanelType[] {
                        PanelType.TOP_SITES,
                        PanelType.BOOKMARKS,
                        PanelType.COMBINED_HISTORY,
                },
                new PanelType[] {
                        // Last version: no migration exists yet, we only need to define a list
                        // of expected panels.
                }
        ));
    }

    private JSONArray createDisabledConfigsForList(Context context,
                                                   PanelType[] panels) throws JSONException {
        final JSONArray jsonPanels = new JSONArray();

        for (int i = 0; i < panels.length; i++) {
            final PanelType panel = panels[i];

            jsonPanels.put(HomeConfig.createBuiltinPanelConfig(context, panel,
                    EnumSet.of(PanelConfig.Flags.DISABLED_PANEL)).toJSON());
        }

        return jsonPanels;

    }


    private JSONArray createConfigsForList(Context context, PanelType[] panels,
                                           int defaultIndex) throws JSONException {
        if (defaultIndex < 0 || defaultIndex >= panels.length) {
            throw new IllegalArgumentException("defaultIndex must point to panel in the array");
        }

        final JSONArray jsonPanels = new JSONArray();

        for (int i = 0; i < panels.length; i++) {
            final PanelType panel = panels[i];
            final PanelConfig config;

            if (i == defaultIndex) {
                config = HomeConfig.createBuiltinPanelConfig(context, panel,
                        EnumSet.of(PanelConfig.Flags.DEFAULT_PANEL));
            } else {
                config = HomeConfig.createBuiltinPanelConfig(context, panel);
            }

            jsonPanels.put(config.toJSON());
        }

        return jsonPanels;
    }

    private PanelType getDefaultPanel(final JSONArray jsonPanels) throws JSONException {
        assertTrue("panel list must not be empty", jsonPanels.length() > 0);

        for (int i = 0; i < jsonPanels.length(); i++) {
            final JSONObject jsonPanelConfig = jsonPanels.getJSONObject(i);
            final PanelConfig panelConfig = new PanelConfig(jsonPanelConfig);

            if (panelConfig.isDefault()) {
                return panelConfig.getType();
            }
        }

        return null;
    }

    private void checkAllPanelsAreDisabled(JSONArray jsonPanels) throws JSONException {
        for (int i = 0; i < jsonPanels.length(); i++) {
            final JSONObject jsonPanelConfig = jsonPanels.getJSONObject(i);
            final PanelConfig config = new PanelConfig(jsonPanelConfig);

            assertTrue("Non disabled panel \"" + config.getType().name() + "\" found in list, excpected all panels to be disabled", config.isDisabled());
        }
    }

    private void checkListContainsExpectedPanels(JSONArray jsonPanels,
                                                 PanelType[] expected) throws JSONException {
        // Given the short lists we have here an ArraySet might be more appropriate, but it requires API >= 23.
        final Set<PanelType> expectedSet = new HashSet<>();
        for (PanelType panelType : expected) {
            expectedSet.add(panelType);
        }

        for (int i = 0; i < jsonPanels.length(); i++) {
            final JSONObject jsonPanelConfig = jsonPanels.getJSONObject(i);
            final PanelType panelType = new PanelConfig(jsonPanelConfig).getType();

            assertTrue("Unexpected panel of type " + panelType.name() + " found in list",
                    expectedSet.contains(panelType));

            expectedSet.remove(panelType);
        }

        assertEquals("Expected panels not contained in list",
                0, expectedSet.size());
    }

    @Test
    public void testMigrationRetainsDefaultAfter6() throws JSONException {
        final Context context = RuntimeEnvironment.application;

        final Pair<PanelType[], PanelType[]> finalConstellation = migrationConstellations.get(HomeConfigPrefsBackend.VERSION);
        assertNotNull("It looks like you added a HomeConfig migration, please add an appropriate entry to migrationConstellations",
                finalConstellation);

        // We want to calculate the number of iterations here to make sure we cover all provided constellations.
        // Iterating over the array and manually checking for each version could result in constellations
        // being skipped if there are any gaps in the array
        final int firstTestedVersion = HomeConfigPrefsBackend.VERSION - (migrationConstellations.size() - 1);

        // The last constellation is only used for the counts / expected outputs, hence we start
        // with the second-last constellation
        for (int testVersion = HomeConfigPrefsBackend.VERSION - 1; testVersion >= firstTestedVersion; testVersion--) {

            final Pair<PanelType[], PanelType[]> currentConstellation = migrationConstellations.get(testVersion);
            assertNotNull("No constellation for version " + testVersion + " - you must provide a constellation for every version upgrade in the list",
                    currentConstellation);

            final PanelType[] inputList = currentConstellation.first;
            final PanelType[] expectedDefaults = currentConstellation.second;

            for (int i = 0; i < inputList.length; i++) {
                JSONArray jsonPanels = createConfigsForList(context, inputList, i);


                // Verify that we still have a default panel, and that it is the expected default panel

                // No need to pass in the prefsEditor since that is only used for the 0->1 migration
                jsonPanels = HomeConfigPrefsBackend.migratePrefsFromVersionToVersion(context, testVersion, testVersion + 1, jsonPanels, null);

                final PanelType oldDefaultPanelType = inputList[i];
                final PanelType expectedNewDefaultPanelType = expectedDefaults[i];
                final PanelType newDefaultPanelType = getDefaultPanel(jsonPanels);

                assertNotNull("No default panel set when migrating from " + testVersion + " to " + testVersion + 1 + ", with previous default as " + oldDefaultPanelType.name(),
                        newDefaultPanelType);

                assertEquals("Migration changed to unexpected default panel - migrating from " + oldDefaultPanelType.name() + ", expected " +  expectedNewDefaultPanelType.name() + " but got " + newDefaultPanelType.name(),
                        newDefaultPanelType, expectedNewDefaultPanelType);


                // Verify that the panels remaining after the migration correspond to the input panels
                // for the next migration
                final PanelType[] expectedOutputList = migrationConstellations.get(testVersion + 1).first;

                assertEquals("Number of panels after migration doesn't match expected count",
                        jsonPanels.length(), expectedOutputList.length);

                checkListContainsExpectedPanels(jsonPanels, expectedOutputList);
            }
        }
    }

    // Test that if all panels are disabled, the migration retains all panels as being disabled
    // (in addition to correctly removing panels as necessary).
    @Test
    public void testMigrationRetainsAllPanelsHiddenAfter6() throws JSONException {
        final Context context = RuntimeEnvironment.application;

        final Pair<PanelType[], PanelType[]> finalConstellation = migrationConstellations.get(HomeConfigPrefsBackend.VERSION);
        assertNotNull("It looks like you added a HomeConfig migration, please add an appropriate entry to migrationConstellations",
                finalConstellation);

        final int firstTestedVersion = HomeConfigPrefsBackend.VERSION - (migrationConstellations.size() - 1);

        for (int testVersion = HomeConfigPrefsBackend.VERSION - 1; testVersion >= firstTestedVersion; testVersion--) {
            final Pair<PanelType[], PanelType[]> currentConstellation = migrationConstellations.get(testVersion);
            assertNotNull("No constellation for version " + testVersion + " - you must provide a constellation for every version upgrade in the list",
                    currentConstellation);

            final PanelType[] inputList = currentConstellation.first;

            JSONArray jsonPanels = createDisabledConfigsForList(context, inputList);

            jsonPanels = HomeConfigPrefsBackend.migratePrefsFromVersionToVersion(context, testVersion, testVersion + 1, jsonPanels, null);

            // All panels should remain disabled after the migration
            checkAllPanelsAreDisabled(jsonPanels);

            // Duplicated from previous test:
            // Verify that the panels remaining after the migration correspond to the input panels
            // for the next migration
            final PanelType[] expectedOutputList = migrationConstellations.get(testVersion + 1).first;

            assertEquals("Number of panels after migration doesn't match expected count",
                    jsonPanels.length(), expectedOutputList.length);

            checkListContainsExpectedPanels(jsonPanels, expectedOutputList);
        }
    }
}
