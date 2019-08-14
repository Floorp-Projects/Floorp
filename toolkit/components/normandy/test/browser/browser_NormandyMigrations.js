ChromeUtils.import("resource://normandy/NormandyMigrations.jsm", this);

decorate_task(withMockPreferences, async function testApplyMigrations(
  mockPreferences
) {
  const migrationsAppliedPref = "app.normandy.migrationsApplied";
  mockPreferences.set(migrationsAppliedPref, 0);

  await NormandyMigrations.applyAll();

  is(
    Services.prefs.getIntPref(migrationsAppliedPref),
    NormandyMigrations.migrations.length,
    "All migrations should have been applied"
  );
});

decorate_task(withMockPreferences, async function testPrefMigration(
  mockPreferences
) {
  const legacyPref = "extensions.shield-recipe-client.test";
  const migratedPref = "app.normandy.test";
  mockPreferences.set(legacyPref, 1);

  ok(
    Services.prefs.prefHasUserValue(legacyPref),
    "Legacy pref should have a user value before running migration"
  );
  ok(
    !Services.prefs.prefHasUserValue(migratedPref),
    "Migrated pref should not have a user value before running migration"
  );

  await NormandyMigrations.applyOne(0);

  ok(
    !Services.prefs.prefHasUserValue(legacyPref),
    "Legacy pref should not have a user value after running migration"
  );
  ok(
    Services.prefs.prefHasUserValue(migratedPref),
    "Migrated pref should have a user value after running migration"
  );
  is(
    Services.prefs.getIntPref(migratedPref),
    1,
    "Value should have been migrated"
  );

  Services.prefs.clearUserPref(migratedPref);
});

decorate_task(withMockPreferences, async function testMigration0(
  mockPreferences
) {
  const studiesEnabledPref = "app.shield.optoutstudies.enabled";
  const healthReportUploadEnabledPref =
    "datareporting.healthreport.uploadEnabled";

  // Both enabled
  mockPreferences.set(studiesEnabledPref, true);
  mockPreferences.set(healthReportUploadEnabledPref, true);
  await NormandyMigrations.applyOne(1);
  ok(
    Services.prefs.getBoolPref(studiesEnabledPref),
    "Studies should be enabled."
  );

  mockPreferences.cleanup();

  // Telemetry disabled, studies enabled
  mockPreferences.set(studiesEnabledPref, true);
  mockPreferences.set(healthReportUploadEnabledPref, false);
  await NormandyMigrations.applyOne(1);
  ok(
    !Services.prefs.getBoolPref(studiesEnabledPref),
    "Studies should be disabled."
  );

  mockPreferences.cleanup();

  // Telemetry enabled, studies disabled
  mockPreferences.set(studiesEnabledPref, false);
  mockPreferences.set(healthReportUploadEnabledPref, true);
  await NormandyMigrations.applyOne(1);
  ok(
    !Services.prefs.getBoolPref(studiesEnabledPref),
    "Studies should be disabled."
  );

  mockPreferences.cleanup();

  // Both disabled
  mockPreferences.set(studiesEnabledPref, false);
  mockPreferences.set(healthReportUploadEnabledPref, false);
  await NormandyMigrations.applyOne(1);
  ok(
    !Services.prefs.getBoolPref(studiesEnabledPref),
    "Studies should be disabled."
  );
});
