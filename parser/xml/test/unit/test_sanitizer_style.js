const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

if (AppConstants.platform != "android") {
  // We load HTML documents, which try to track link state, which requires
  // the history service, which requires a profile.
  do_get_profile();
}

const kTestCases = [
  {
    // bug 1602843
    data: `@font-face { font-family: 'ab<\\/style><img src onerror=alert(1)>'}`,
    sanitized: `@font-face { font-family: 'ab<\\/style><img src onerror=alert(1)>'}`,
  },
  {
    // bug 1680084
    data: `<!--
/* Font Definitions */
@font-face
       {font-family:"Cambria Math";
       panose-1:2 4 5 3 5 4 6 3 2 4;}
@font-face
       {font-family:"Yu Gothic";
       panose-1:2 11 4 0 0 0 0 0 0 0;}
@font-face
       {font-family:"Yu Gothic";
       panose-1:2 11 4 0 0 0 0 0 0 0;}
/* Style Definitions */
p.MsoNormal, li.MsoNormal, div.MsoNormal
       {margin:0mm;
       text-align:justify;
       text-justify:inter-ideograph;
       font-size:10.5pt;
       font-family:"Yu Gothic";}
span.17
       {mso-style-type:personal-compose;
       font-family:"Yu Gothic";
       color:windowtext;}
.MsoChpDefault
       {mso-style-type:export-only;
       font-family:"Yu Gothic";}
/* Page Definitions */
@page WordSection1
       {size:612.0pt 792.0pt;
       margin:99.25pt 30.0mm 30.0mm 30.0mm;}
div.WordSection1
       {page:WordSection1}
-->`,
    sanitized: `@font-face
       {font-family:"Cambria Math";
       panose-1:2 4 5 3 5 4 6 3 2 4;}@font-face
       {font-family:"Yu Gothic";
       panose-1:2 11 4 0 0 0 0 0 0 0;}@font-face
       {font-family:"Yu Gothic";
       panose-1:2 11 4 0 0 0 0 0 0 0;}p.MsoNormal, li.MsoNormal, div.MsoNormal
       {margin:0mm;
       text-align:justify;
       text-justify:inter-ideograph;
       font-size:10.5pt;
       font-family:"Yu Gothic";}.MsoChpDefault
       {mso-style-type:export-only;
       font-family:"Yu Gothic";}div.WordSection1
       {page:WordSection1}`,
  },
];

const kConditionalCSSTestCases = [
  {
    data: `#foo { display: none } @media (min-width: 300px) { #bar { display: none } }`,
    sanitized: `#foo { display: none }`,
  },
];

function run_test() {
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
    ParserUtils.SanitizerDropForms |
    ParserUtils.SanitizerDropNonCSSPresentation |
    ParserUtils.SanitizerAllowStyle;

  for (let { data, sanitized } of kTestCases) {
    let out = ParserUtils.sanitize(`<style>${data}</style>`, sanitizeFlags);
    info(out);
    Assert.equal(
      `<html><head><style>${sanitized}</style></head><body></body></html>`,
      out
    );
  }

  for (let { data, sanitized } of kConditionalCSSTestCases) {
    let out = ParserUtils.removeConditionalCSS(`<style>${data}</style>`);
    info(out);
    Assert.equal(
      `<html><head><style>${sanitized}</style></head><body></body></html>`,
      out
    );
  }
}
