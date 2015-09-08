// This file is needed to work around a Firefox bug where capability.principal 
// prefs in user.js don't get recognized until the second browser launch
// which is too late for our purposes of using quit.js. Loading the principals 
// from prefs.js avoids this issue.
user_pref("capability.principal.codebase.p0.granted", "UniversalPreferencesWrite UniversalXPConnect UniversalPreferencesRead");
user_pref("capability.principal.codebase.p0.id", "file://");
user_pref("capability.principal.codebase.p0.subjectName", "");
user_pref("capability.principal.codebase.p1.granted", "UniversalPreferencesWrite UniversalXPConnect UniversalPreferencesRead");
user_pref("capability.principal.codebase.p1.id", "http://localhost");
user_pref("capability.principal.codebase.p1.subjectName", "");
user_pref("signed.applets.codebase_principal_support", true);
