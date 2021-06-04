/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// useMLBF=true doesn't support soft blocks, regexps or version ranges.
// Flip the useMLBF preference to make sure that the test_blocklistchange.js
// test works with and without this pref (blocklist v2 and blocklist v3).
//
// This is a bit of a hack and to be replaced with a new file in bug 1649906.
Services.prefs.setBoolPref(
  "extensions.blocklist.useMLBF",
  !Services.prefs.getBoolPref("extensions.blocklist.useMLBF")
);

// The test requires stashes to be enabled.
Services.prefs.setBoolPref("extensions.blocklist.useMLBF.stashes", true);

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("test_blocklistchange.js")).spec,
  this
);
