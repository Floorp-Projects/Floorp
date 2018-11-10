// Preferences file used by the raptor harness
/* globals user_pref */
user_pref("dom.performance.time_to_non_blank_paint.enabled", true);
user_pref("dom.performance.time_to_dom_content_flushed.enabled", true);
user_pref("dom.performance.time_to_first_interactive.enabled", true);

// required for geckoview logging
user_pref("geckoview.console.enabled", true);

// required to prevent non-local access to push.services.mozilla.com
user_pref("dom.push.connection.enabled", false);
