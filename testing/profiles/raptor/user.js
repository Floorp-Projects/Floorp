// Preferences file used by the raptor harness
/* globals user_pref */
user_pref("dom.performance.time_to_non_blank_paint.enabled", true);
user_pref("dom.performance.time_to_contentful_paint.enabled", true);
user_pref("dom.performance.time_to_dom_content_flushed.enabled", true);
user_pref("dom.performance.time_to_first_interactive.enabled", true);

// required for geckoview logging
user_pref("geckoview.console.enabled", true);

// required to prevent non-local access to push.services.mozilla.com
user_pref("dom.push.connection.enabled", false);

// get the console logging out of the webext into the stdout
user_pref("browser.dom.window.dump.enabled", true);
user_pref("devtools.console.stdout.chrome", true);
user_pref("devtools.console.stdout.content", true);

// prevent pages from opening after a crash
user_pref("browser.sessionstore.resume_from_crash", false);
