function run_test() {
  var Ci = Components.interfaces;
  var Cc = Components.classes;

  // vectors by the html5security project (https://code.google.com/p/html5security/ & Creative Commons 3.0 BY), see CC-BY-LICENSE for the full license
  load("results.js");   // gives us a `vectors' array

  var ParserUtils =  Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);
  var sanitizeFlags = ParserUtils.SanitizerCidEmbedsOnly|ParserUtils.SanitizerDropForms|ParserUtils.SanitizerDropNonCSSPresentation;
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
