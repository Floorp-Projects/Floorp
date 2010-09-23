pref("services.sync.serverURL", "https://auth.services.mozilla.com/");
pref("services.sync.storageAPI", "1.0");
pref("services.sync.userURL", "user/");
pref("services.sync.miscURL", "misc/");
pref("services.sync.termsURL", "https://services.mozilla.com/tos/");
pref("services.sync.privacyURL", "https://services.mozilla.com/privacy-policy/");
pref("services.sync.statusURL", "https://services.mozilla.com/status/");
pref("services.sync.syncKeyHelpURL", "https://services.mozilla.com/help/synckey");

pref("services.sync.lastversion", "firstrun");
pref("services.sync.autoconnect", true);

pref("services.sync.engine.bookmarks", true);
pref("services.sync.engine.history", true);
pref("services.sync.engine.passwords", true);
pref("services.sync.engine.prefs", true);
pref("services.sync.engine.tabs", true);
pref("services.sync.engine.tabs.filteredUrls", "^(about:.*|chrome://weave/.*|wyciwyg:.*|file:.*)$");

pref("services.sync.log.appender.console", "Warn");
pref("services.sync.log.appender.dump", "Error");
pref("services.sync.log.appender.debugLog", "Trace");
pref("services.sync.log.rootLogger", "Debug");
pref("services.sync.log.logger.service.main", "Debug");
pref("services.sync.log.logger.authenticator", "Debug");
pref("services.sync.log.logger.network.resources", "Debug");
pref("services.sync.log.logger.engine.bookmarks", "Debug");
pref("services.sync.log.logger.engine.clients", "Debug");
pref("services.sync.log.logger.engine.forms", "Debug");
pref("services.sync.log.logger.engine.history", "Debug");
pref("services.sync.log.logger.engine.passwords", "Debug");
pref("services.sync.log.logger.engine.prefs", "Debug");
pref("services.sync.log.logger.engine.tabs", "Debug");
pref("services.sync.log.cryptoDebug", false);
