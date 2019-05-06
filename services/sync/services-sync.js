/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global pref */

pref("services.sync.lastversion", "firstrun");
pref("services.sync.sendVersionInfo", true);

pref("services.sync.scheduler.idleInterval", 3600);  // 1 hour
pref("services.sync.scheduler.activeInterval", 600);   // 10 minutes
pref("services.sync.scheduler.immediateInterval", 90);    // 1.5 minutes
pref("services.sync.scheduler.idleTime", 300);   // 5 minutes

pref("services.sync.scheduler.fxa.singleDeviceInterval", 3600); // 1 hour

// Note that new engines are typically added with a default of disabled, so
// when an existing sync user gets the Firefox upgrade that supports the engine
// it starts as disabled until the user has explicitly opted in.
// The sync "create account" process typically *will* offer these engines, so
// they may be flipped to enabled at that time.
pref("services.sync.engine.addons", true);
pref("services.sync.engine.addresses", false);
pref("services.sync.engine.bookmarks", true);
#ifdef EARLY_BETA_OR_EARLIER
// Enable the new bookmark sync engine through early Beta, but not release
// candidates or Release.
pref("services.sync.engine.bookmarks.buffer", true);
#else
pref("services.sync.engine.bookmarks.buffer", false);
#endif
pref("services.sync.engine.creditcards", false);
pref("services.sync.engine.history", true);
pref("services.sync.engine.passwords", true);
pref("services.sync.engine.prefs", true);
pref("services.sync.engine.tabs", true);
pref("services.sync.engine.tabs.filteredUrls", "^(about:.*|resource:.*|chrome:.*|wyciwyg:.*|file:.*|blob:.*|moz-extension:.*)$");

// The addresses and CC engines might not actually be available at all.
pref("services.sync.engine.addresses.available", false);
pref("services.sync.engine.creditcards.available", false);

// If true, add-on sync ignores changes to the user-enabled flag. This
// allows people to have the same set of add-ons installed across all
// profiles while maintaining different enabled states.
pref("services.sync.addons.ignoreUserEnabledChanges", false);

// Comma-delimited list of hostnames to trust for add-on install.
pref("services.sync.addons.trustedSourceHostnames", "addons.mozilla.org");

pref("services.sync.log.appender.console", "Fatal");
pref("services.sync.log.appender.dump", "Error");
pref("services.sync.log.appender.file.level", "Trace");
pref("services.sync.log.appender.file.logOnError", true);
#if defined(NIGHTLY_BUILD)
pref("services.sync.log.appender.file.logOnSuccess", true);
#else
pref("services.sync.log.appender.file.logOnSuccess", false);
#endif
pref("services.sync.log.appender.file.maxErrorAge", 864000); // 10 days

// The default log level for all "Sync.*" logs. Adjusting this pref will
// adjust the level for *all* Sync logs (except engines, and that's only
// because we supply a default for the engines below.)
pref("services.sync.log.logger", "Debug");

// Prefs for Sync engines can be controlled globally or per-engine.
// We only define the global level here, but manually creating prefs
// like "services.sync.log.logger.engine.bookmarks" will control just
// that engine.
pref("services.sync.log.logger.engine", "Debug");
pref("services.sync.log.cryptoDebug", false);

pref("services.sync.fxa.termsURL", "https://accounts.firefox.com/legal/terms");
pref("services.sync.fxa.privacyURL", "https://accounts.firefox.com/legal/privacy");

pref("services.sync.telemetry.submissionInterval", 43200); // 12 hours in seconds
pref("services.sync.telemetry.maxPayloadCount", 500);

#ifdef EARLY_BETA_OR_EARLIER
// Enable the (fairly costly) client/server validation through early Beta, but
// not release candidates or Release.
pref("services.sync.engine.bookmarks.validation.enabled", true);
pref("services.sync.engine.passwords.validation.enabled", true);
#endif

#if defined(NIGHTLY_BUILD)
// Enable repair of bookmarks on Nightly only - requires validation also be
// enabled.
pref("services.sync.engine.bookmarks.repair.enabled", true);
#endif

// We consider validation this frequently. After considering validation, even
// if we don't end up validating, we won't try again unless this much time has passed.
pref("services.sync.engine.bookmarks.validation.interval", 86400); // 24 hours in seconds
pref("services.sync.engine.passwords.validation.interval", 86400); // 24 hours in seconds

// We only run validation `services.sync.validation.percentageChance` percent of
// the time, even if it's been the right amount of time since the last validation,
// and you meet the maxRecord checks.
pref("services.sync.engine.bookmarks.validation.percentageChance", 10);
pref("services.sync.engine.passwords.validation.percentageChance", 10);

// We won't validate an engine if it has more than this many records on the server.
pref("services.sync.engine.bookmarks.validation.maxRecords", 1000);
pref("services.sync.engine.passwords.validation.maxRecords", 1000);

// The maximum number of immediate resyncs to trigger for changes made during
// a sync.
pref("services.sync.maxResyncs", 5);
