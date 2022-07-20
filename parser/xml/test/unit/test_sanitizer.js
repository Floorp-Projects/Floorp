const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
if (AppConstants.platform != "android") {
  // We load HTML documents, which try to track link state, which requires
  // the history service, which requires a profile.
  do_get_profile();
}

function run_test() {
  // vectors by the html5security project (https://code.google.com/p/html5security/ & Creative Commons 3.0 BY), see CC-BY-LICENSE for the full license
  load("results.js"); // gives us a `vectors' array
  /* import-globals-from ./results.js */

  if (AppConstants.platform != "android") {
    // xpcshell tests are weird. They fake shutdown after the test finishes. This upsets this test
    // because it will try to create the history service to check for visited state on the links
    // we're parsing.
    // Creating the history service midway through shutdown breaks.
    // We can't catch this in the history component because we're not *actually* shutting down,
    // and so the app startup's service's `shuttingDown` bool is false, even though normally that
    // is set to true *before* profile-change-teardown notifications are fired.
    // To work around this, just force the history service to be created earlier:

    let { PlacesUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PlacesUtils.sys.mjs"
    );
    Assert.ok(
      PlacesUtils.history.databaseStatus <= 1,
      "ensure places database is successfully initialized."
    );
  }

  var ParserUtils = Cc["@mozilla.org/parserutils;1"].getService(
    Ci.nsIParserUtils
  );
  var sanitizeFlags =
    ParserUtils.SanitizerCidEmbedsOnly |
    ParserUtils.SanitizerDropForms |
    ParserUtils.SanitizerDropNonCSSPresentation;
  // flags according to
  // http://mxr.mozilla.org/comm-central/source/mailnews/mime/src/mimemoz2.cpp#2218
  // and default settings

  for (var item in vectors) {
    var evil = vectors[item].data;
    var sanitized = vectors[item].sanitized;
    var out = ParserUtils.sanitize(evil, sanitizeFlags);
    Assert.equal(sanitized, out);
  }
}
