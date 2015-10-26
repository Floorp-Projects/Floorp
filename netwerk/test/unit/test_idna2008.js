const kTransitionalProcessing = false;

// Four characters map differently under non-transitional processing:
const labels = [
  // U+00DF LATIN SMALL LETTER SHARP S to "ss"
  "stra\u00dfe",
  // U+03C2 GREEK SMALL LETTER FINAL SIGMA to U+03C3 GREEK SMALL LETTER SIGMA
  "\u03b5\u03bb\u03bb\u03ac\u03c2",
  // U+200C ZERO WIDTH NON-JOINER in Indic script
  "\u0646\u0627\u0645\u0647\u200c\u0627\u06cc",
  // U+200D ZERO WIDTH JOINER in Arabic script
  "\u0dc1\u0dca\u200d\u0dbb\u0dd3",

  // But CONTEXTJ rules prohibit ZWJ and ZWNJ in non-Arabic or Indic scripts
  // U+200C ZERO WIDTH NON-JOINER in Latin script
  "m\200cn",
  // U+200D ZERO WIDTH JOINER in Latin script
  "p\200dq",
];

const transitionalExpected = [
  "strasse",
  "xn--hxarsa5b",
  "xn--mgba3gch31f",
  "xn--10cl1a0b",
  "",
  ""
];

const nonTransitionalExpected = [
  "xn--strae-oqa",
  "xn--hxarsa0b",
  "xn--mgba3gch31f060k",
  "xn--10cl1a0b660p",
  "",
  ""
];

// Test options for converting IDN URLs under IDNA2008
function run_test()
{
  var idnService = Components.classes["@mozilla.org/network/idn-service;1"]
                             .getService(Components.interfaces.nsIIDNService);


  for (var i = 0; i < labels.length; ++i) {
    var result;
    try {
        result = idnService.convertUTF8toACE(labels[i]);
    } catch(e) {
        result = "";
    }

    if (kTransitionalProcessing) {
      equal(result, transitionalExpected[i]);
    } else {
      equal(result, nonTransitionalExpected[i]);
    }
  }
}
