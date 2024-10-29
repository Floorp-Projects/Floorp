// Base preferences file used by the mochitest
/* globals user_pref */
/* eslint quotes: 0 */

// Don't restore the last open set of tabs if the browser has crashed
// or if the profile folder is re-used after an exit(0) fast shutdown.
user_pref("browser.sessionstore.resume_from_crash", false);

// Better stacks for errors.
user_pref("javascript.options.asyncstack_capture_debuggee_only", false);

// Don't enable paint suppression when the background is unknown. While paint
// is suppressed, synthetic click events and co. go to the old page, which can
// be confusing for tests that send click events before the first paint.
user_pref("nglayout.initialpaint.unsuppress_with_no_background", true);

// Disable prefers-reduced-motion to ensure that smooth scrolls can be tested.
user_pref("general.smoothScroll", true);

// Disable secure context pref for testing storage access API.
// Related Bug: https://bugzilla.mozilla.org/show_bug.cgi?id=1840902.
// Plan to remove this pref since all tests must be compliant with the
// storage access API spec for secure contexts.
user_pref("dom.storage_access.dont_grant_insecure_contexts", false);

// Turn off update
user_pref("app.update.disabledForTesting", true);

// This feature restricts to add history when accessing to web page is too
// frequently, it assumes real human accesses them by clicking and key types.
// Therefore, in the mochitest, as the frequently in common browser tests can be
// super higher than the real user, we disable this feature.
user_pref("places.history.floodingPrevention.enabled", false);

// If we are on a platform where we can detect that we don't have OS geolocation
// permission, and we can open it and wait for the user to give permission, then
// don't do that.
user_pref("geo.prompt.open_system_prefs", false);
