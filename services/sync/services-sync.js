pref("extensions.weave.serverURL", "@server_url@");
pref("extensions.weave.miscURL", "https://auth.services.mozilla.com/misc/");
pref("extensions.weave.termsURL", "https://labs.mozilla.com/projects/weave/tos/");

pref("extensions.weave.encryption", "aes-256-cbc");

pref("extensions.weave.lastversion", "firstrun");

pref("extensions.weave.ui.syncnow", true);
pref("extensions.weave.ui.sharebookmarks", false);
pref("extensions.weave.rememberpassword", true);
pref("extensions.weave.autoconnect", true);
pref("extensions.weave.enabled", true);
pref("extensions.weave.schedule", 1);

pref("extensions.weave.syncOnQuit.enabled", true);

pref("extensions.weave.engine.bookmarks", true);
//pref("extensions.weave.engine.cookies", false);
pref("extensions.weave.engine.forms", true);
pref("extensions.weave.engine.history", true);
//pref("extensions.weave.engine.input", false);
pref("extensions.weave.engine.passwords", true);
pref("extensions.weave.engine.prefs", true);
pref("extensions.weave.engine.tabs", true);

pref("extensions.weave.log.appender.console", "Warn");
pref("extensions.weave.log.appender.dump", "Error");
pref("extensions.weave.log.appender.briefLog", "Info");
pref("extensions.weave.log.appender.debugLog", "Trace");

pref("extensions.weave.log.rootLogger", "Trace");
pref("extensions.weave.log.logger.service.main", "Trace");
pref("extensions.weave.log.logger.async", "Debug");
pref("extensions.weave.log.logger.authenticator", "Debug");
pref("extensions.weave.log.logger.network.resources", "Debug");
pref("extensions.weave.log.logger.engine.bookmarks", "Debug");
pref("extensions.weave.log.logger.engine.clients", "Debug");
pref("extensions.weave.log.logger.engine.forms", "Debug");
pref("extensions.weave.log.logger.engine.history", "Debug");
pref("extensions.weave.log.logger.engine.passwords", "Debug");
pref("extensions.weave.log.logger.engine.tabs", "Debug");
pref("extensions.weave.log.logger.authenticator", "Debug");

pref("extensions.weave.network.numRetries", 2);

pref("extensions.weave.tabs.sortMode", "recency");
pref("extensions.weave.openId.enabled", true);
pref("extensions.weave.authenticator.enabled", true);

// Preferences to be synced by default
pref("extensions.weave.prefs.sync.browser.download.manager.scanWhenDone", true);

pref("extensions.weave.prefs.sync.browser.search.openintab", true);
pref("extensions.weave.prefs.sync.browser.search.selectedEngine", true);

pref("extensions.weave.prefs.sync.browser.sessionstore.resume_from_crash", true);
pref("extensions.weave.prefs.sync.browser.sessionstore.resume_session_once", true);
pref("extensions.weave.prefs.sync.browser.sessionstore.interval", true);

pref("extensions.weave.prefs.sync.browser.startup.homepage", true);
pref("extensions.weave.prefs.sync.startup.homepage_override_url", true);

pref("extensions.weave.prefs.sync.browser.tabs.tabMinWidth", true);
pref("extensions.weave.prefs.sync.browser.tabs.warnOnClose", true);
pref("extensions.weave.prefs.sync.browser.tabs.closeButtons", true);

pref("extensions.weave.prefs.sync.browser.urlbar.autoFill", true);
pref("extensions.weave.prefs.sync.browser.urlbar.maxRichResults", true);
pref("extensions.weave.prefs.sync.browser.urlbar.clickSelectsAll", true);
pref("extensions.weave.prefs.sync.browser.urlbar.doubleClickSelectsAll", true);

pref("extensions.weave.prefs.sync.extensions.personas.current", true);

pref("extensions.weave.prefs.sync.layout.spellcheckDefault", true);
pref("extensions.weave.prefs.sync.security.dialog_enable_delay", true);
pref("extensions.weave.prefs.sync.signon.rememberSignons", true);
pref("extensions.weave.prefs.sync.spellchecker.dictionary", true);
