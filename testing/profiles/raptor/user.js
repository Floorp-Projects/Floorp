// Preferences file used by the raptor harness
/* globals user_pref */
// prevents normandy from running updates during the tests
user_pref("app.normandy.enabled", false);

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

// disable the background hang monitor
user_pref("toolkit.content-background-hang-monitor.disabled", true);

// disable async stacks to match release builds
// https://developer.mozilla.org/en-US/docs/Mozilla/Benchmarking#Async_Stacks
user_pref('javascript.options.asyncstack', false);

// disable Firefox Telemetry (and some other things too)
// https://bugzilla.mozilla.org/show_bug.cgi?id=1533879
user_pref('datareporting.healthreport.uploadEnabled', false);
