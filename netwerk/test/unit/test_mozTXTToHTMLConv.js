/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that mozITXTToHTMLConv works properly.
 */

"use strict";

function run_test() {
  let converter = Cc["@mozilla.org/txttohtmlconv;1"].getService(
    Ci.mozITXTToHTMLConv
  );

  const scanTXTtests = [
    // -- RFC1738
    {
      input: "RFC1738: <URL:http://mozilla.org> then",
      url: "http://mozilla.org",
    },
    {
      input: "RFC1738: <URL:mailto:john.doe+test@mozilla.org> then",
      url: "mailto:john.doe+test@mozilla.org",
    },
    // -- RFC2396E
    {
      input: "RFC2396E: <http://mozilla.org/> then",
      url: "http://mozilla.org/",
    },
    {
      input: "RFC2396E: <john.doe+test@mozilla.org> then",
      url: "mailto:john.doe+test@mozilla.org",
    },
    // -- abbreviated
    {
      input: "see www.mozilla.org maybe",
      url: "http://www.mozilla.org",
    },
    {
      input: "mail john.doe+test@mozilla.org maybe",
      url: "mailto:john.doe+test@mozilla.org",
    },
    // -- delimiters
    {
      input: "see http://www.mozilla.org/maybe today", // Spaces
      url: "http://www.mozilla.org/maybe",
    },
    {
      input: 'see "http://www.mozilla.org/maybe today"', // Double quotes
      url: "http://www.mozilla.org/maybetoday", // spaces ignored
    },
    {
      input: "see <http://www.mozilla.org/maybe today>", // Angle brackets
      url: "http://www.mozilla.org/maybetoday", // spaces ignored
    },
    // -- freetext
    {
      input: "I mean http://www.mozilla.org/.",
      url: "http://www.mozilla.org/",
    },
    {
      input: "you mean http://mozilla.org:80, right?",
      url: "http://mozilla.org:80",
    },
    {
      input: "go to http://mozilla.org; then go home",
      url: "http://mozilla.org",
    },
    {
      input: "http://mozilla.org! yay!",
      url: "http://mozilla.org",
    },
    {
      input: "er, http://mozilla.com?",
      url: "http://mozilla.com",
    },
    {
      input: "http://example.org- where things happen",
      url: "http://example.org",
    },
    {
      input: "see http://mozilla.org: front page",
      url: "http://mozilla.org",
    },
    {
      input: "'http://mozilla.org/': that's the url",
      url: "http://mozilla.org/",
    },
    {
      input: "some special http://mozilla.org/?x=.,;!-:x",
      url: "http://mozilla.org/?x=.,;!-:x",
    },
    {
      // escape & when producing html
      input: "'http://example.org/?test=true&success=true': ok",
      url: "http://example.org/?test=true&amp;success=true",
    },
    {
      input: "bracket: http://localhost/[1] etc.",
      url: "http://localhost/",
    },
    {
      input: "bracket: john.doe+test@mozilla.org[1] etc.",
      url: "mailto:john.doe+test@mozilla.org",
    },
    {
      input: "parenthesis: (http://localhost/) etc.",
      url: "http://localhost/",
    },
    {
      input: "parenthesis: (john.doe+test@mozilla.org) etc.",
      url: "mailto:john.doe+test@mozilla.org",
    },
    {
      input: "(thunderbird)http://mozilla.org/thunderbird",
      url: "http://mozilla.org/thunderbird",
    },
    {
      input: "(mail)john.doe+test@mozilla.org",
      url: "mailto:john.doe+test@mozilla.org",
    },
    {
      input: "()http://mozilla.org",
      url: "http://mozilla.org",
    },
    {
      input:
        "parenthesis included: http://kb.mozillazine.org/Performance_(Thunderbird) etc.",
      url: "http://kb.mozillazine.org/Performance_(Thunderbird)",
    },
    {
      input: "parenthesis slash bracket: (http://localhost/)[1] etc.",
      url: "http://localhost/",
    },
    {
      input: "parenthesis bracket: (http://example.org[1]) etc.",
      url: "http://example.org",
    },
    {
      input: "ipv6 1: https://[1080::8:800:200C:417A]/foo?bar=x test",
      url: "https://[1080::8:800:200C:417A]/foo?bar=x",
    },
    {
      input: "ipv6 2: http://[::ffff:127.0.0.1]/#yay test",
      url: "http://[::ffff:127.0.0.1]/#yay",
    },
    {
      input: "ipv6 parenthesis port: (http://[2001:db8::1]:80/) test",
      url: "http://[2001:db8::1]:80/",
    },
    {
      input:
        "test http://www.map.com/map.php?t=Nova_Scotia&markers=//Not_a_survey||description=plm2 test",
      url:
        "http://www.map.com/map.php?t=Nova_Scotia&amp;markers=//Not_a_survey||description=plm2",
    },
    {
      input: "bug#1509493 (john@mozilla.org)john@mozilla.org test",
      url: "mailto:john@mozilla.org",
      text: "john@mozilla.org",
    },
    {
      input: "bug#1509493 {john@mozilla.org}john@mozilla.org test",
      url: "mailto:john@mozilla.org",
      text: "john@mozilla.org",
    },
  ];

  const scanTXTglyph = [
    // Some "glyph" testing (not exhaustive, the system supports 16 different
    // smiley types).
    {
      input: "this is superscript: x^2",
      results: ["<sup", "2", "</sup>"],
    },
    {
      input: "this is plus-minus: +/-",
      results: ["&plusmn;"],
    },
    {
      input: "this is a smiley :)",
      results: ["moz-smiley-s1"],
    },
    {
      input: "this is a smiley :-)",
      results: ["moz-smiley-s1"],
    },
    {
      input: "this is a smiley :-(",
      results: ["moz-smiley-s2"],
    },
  ];

  const scanTXTstrings = [
    "underline", // ASCII
    "äöüßáéíóúî", // Latin-1
    "a\u0301c\u0327c\u030Ce\u0309n\u0303t\u0326e\u0308d\u0323",
    // áçčẻñțëḍ Latin
    "\u016B\u00F1\u0257\u0119\u0211\u0142\u00ED\u00F1\u0119",
    // Pseudo-ese ūñɗęȑłíñę
    "\u01DDu\u0131\u0283\u0279\u01DDpun", // Upside down ǝuıʃɹǝpun
    "\u03C5\u03C0\u03BF\u03B3\u03C1\u03AC\u03BC\u03BC\u03B9\u03C3\u03B7",
    // Greek υπογράμμιση
    "\u0441\u0438\u043B\u044C\u043D\u0443\u044E", // Russian сильную
    "\u0C2C\u0C32\u0C2E\u0C46\u0C56\u0C28", // Telugu బలమైన
    "\u508D\u7DDA\u3059\u308B", // Japanese 傍線する
    "\uD841\uDF0E\uD841\uDF31\uD841\uDF79\uD843\uDC53\uD843\uDC78",
    // Chinese (supplementary plane)
    "\uD801\uDC14\uD801\uDC2F\uD801\uDC45\uD801\uDC28\uD801\uDC49\uD801\uDC2F\uD801\uDC3B",
    // Deseret 𐐔𐐯𐑅𐐨𐑉𐐯𐐻
  ];

  const scanTXTstructs = [
    {
      delimiter: "/",
      tag: "i",
      class: "moz-txt-slash",
    },
    {
      delimiter: "*",
      tag: "b",
      class: "moz-txt-star",
    },
    {
      delimiter: "_",
      tag: "span",
      class: "moz-txt-underscore",
    },
    {
      delimiter: "|",
      tag: "code",
      class: "moz-txt-verticalline",
    },
  ];

  const scanHTMLtests = [
    {
      input: "http://foo.example.com",
      shouldChange: true,
    },
    {
      input: " <a href='http://a.example.com/'>foo</a>",
      shouldChange: false,
    },
    {
      input: "<abbr>see http://abbr.example.com</abbr>",
      shouldChange: true,
    },
    {
      input: "<!-- see http://comment.example.com/ -->",
      shouldChange: false,
    },
    {
      input: "<!-- greater > -->",
      shouldChange: false,
    },
    {
      input: "<!-- lesser < -->",
      shouldChange: false,
    },
    {
      input:
        "<style id='ex'>background-image: url(http://example.com/ex.png);</style>",
      shouldChange: false,
    },
    {
      input: "<style>body > p, body > div { color:blue }</style>",
      shouldChange: false,
    },
    {
      input: "<script>window.location='http://script.example.com/';</script>",
      shouldChange: false,
    },
    {
      input: "<head><title>http://head.example.com/</title></head>",
      shouldChange: false,
    },
    {
      input: "<header>see http://header.example.com</header>",
      shouldChange: true,
    },
    {
      input: "<iframe src='http://iframe.example.com/' />",
      shouldChange: false,
    },
    {
      input: "broken end <script",
      shouldChange: false,
    },
  ];

  function hrefLink(url) {
    return ' href="' + url + '"';
  }

  function linkText(plaintext) {
    return ">" + plaintext + "</a>";
  }

  for (let i = 0; i < scanTXTtests.length; i++) {
    let t = scanTXTtests[i];
    let output = converter.scanTXT(t.input, Ci.mozITXTToHTMLConv.kURLs);
    let link = hrefLink(t.url);
    let text;
    if (t.text) {
      text = linkText(t.text);
    }
    if (!output.includes(link)) {
      do_throw(
        "Unexpected conversion by scanTXT: input=" +
          t.input +
          ", output=" +
          output +
          ", link=" +
          link
      );
    }
    if (text && !output.includes(text)) {
      do_throw(
        "Unexpected conversion by scanTXT: input=" +
          t.input +
          ", output=" +
          output +
          ", text=" +
          text
      );
    }
  }

  for (let i = 0; i < scanTXTglyph.length; i++) {
    let t = scanTXTglyph[i];
    let output = converter.scanTXT(
      t.input,
      Ci.mozITXTToHTMLConv.kGlyphSubstitution
    );
    for (let j = 0; j < t.results.length; j++) {
      if (!output.includes(t.results[j])) {
        do_throw(
          "Unexpected conversion by scanTXT: input=" +
            t.input +
            ", output=" +
            output +
            ", expected=" +
            t.results[j]
        );
      }
    }
  }

  for (let i = 0; i < scanTXTstrings.length; ++i) {
    for (let j = 0; j < scanTXTstructs.length; ++j) {
      let input =
        scanTXTstructs[j].delimiter +
        scanTXTstrings[i] +
        scanTXTstructs[j].delimiter;
      let expected =
        "<" +
        scanTXTstructs[j].tag +
        ' class="' +
        scanTXTstructs[j].class +
        '">' +
        '<span class="moz-txt-tag">' +
        scanTXTstructs[j].delimiter +
        "</span>" +
        scanTXTstrings[i] +
        '<span class="moz-txt-tag">' +
        scanTXTstructs[j].delimiter +
        "</span>" +
        "</" +
        scanTXTstructs[j].tag +
        ">";
      let actual = converter.scanTXT(input, Ci.mozITXTToHTMLConv.kStructPhrase);
      Assert.equal(encodeURIComponent(actual), encodeURIComponent(expected));
    }
  }

  for (let i = 0; i < scanHTMLtests.length; i++) {
    let t = scanHTMLtests[i];
    let output = converter.scanHTML(t.input, Ci.mozITXTToHTMLConv.kURLs);
    let changed = t.input != output;
    if (changed != t.shouldChange) {
      do_throw(
        "Unexpected change by scanHTML: changed=" +
          changed +
          ", shouldChange=" +
          t.shouldChange +
          ", \ninput=" +
          t.input +
          ", \noutput=" +
          output
      );
    }
  }
}
