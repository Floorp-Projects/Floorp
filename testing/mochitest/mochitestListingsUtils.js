/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We expect this to be defined in the global scope by runtest.py.
/* global _TEST_PREFIX */

//
// HTML GENERATION
//
/* global A, ABBR, ACRONYM, ADDRESS, APPLET, AREA, B, BASE,
          BASEFONT, BDO, BIG, BLOCKQUOTE, BODY, BR, BUTTON,
          CAPTION, CENTER, CITE, CODE, COL, COLGROUP, DD,
          DEL, DFN, DIR, DIV, DL, DT, EM, FIELDSET, FONT,
          FORM, FRAME, FRAMESET, H1, H2, H3, H4, H5, H6,
          HEAD, HR, HTML, I, IFRAME, IMG, INPUT, INS,
          ISINDEX, KBD, LABEL, LEGEND, LI, LINK, MAP, MENU,
          META, NOFRAMES, NOSCRIPT, OBJECT, OL, OPTGROUP,
          OPTION, P, PARAM, PRE, Q, S, SAMP, SCRIPT,
          SELECT, SMALL, SPAN, STRIKE, STRONG, STYLE, SUB,
          SUP, TABLE, TBODY, TD, TEXTAREA, TFOOT, TH, THEAD,
          TITLE, TR, TT, U, UL, VAR */
var tags = [
  "A",
  "ABBR",
  "ACRONYM",
  "ADDRESS",
  "APPLET",
  "AREA",
  "B",
  "BASE",
  "BASEFONT",
  "BDO",
  "BIG",
  "BLOCKQUOTE",
  "BODY",
  "BR",
  "BUTTON",
  "CAPTION",
  "CENTER",
  "CITE",
  "CODE",
  "COL",
  "COLGROUP",
  "DD",
  "DEL",
  "DFN",
  "DIR",
  "DIV",
  "DL",
  "DT",
  "EM",
  "FIELDSET",
  "FONT",
  "FORM",
  "FRAME",
  "FRAMESET",
  "H1",
  "H2",
  "H3",
  "H4",
  "H5",
  "H6",
  "HEAD",
  "HR",
  "HTML",
  "I",
  "IFRAME",
  "IMG",
  "INPUT",
  "INS",
  "ISINDEX",
  "KBD",
  "LABEL",
  "LEGEND",
  "LI",
  "LINK",
  "MAP",
  "MENU",
  "META",
  "NOFRAMES",
  "NOSCRIPT",
  "OBJECT",
  "OL",
  "OPTGROUP",
  "OPTION",
  "P",
  "PARAM",
  "PRE",
  "Q",
  "S",
  "SAMP",
  "SCRIPT",
  "SELECT",
  "SMALL",
  "SPAN",
  "STRIKE",
  "STRONG",
  "STYLE",
  "SUB",
  "SUP",
  "TABLE",
  "TBODY",
  "TD",
  "TEXTAREA",
  "TFOOT",
  "TH",
  "THEAD",
  "TITLE",
  "TR",
  "TT",
  "U",
  "UL",
  "VAR",
];

/**
 * Below, we'll use makeTagFunc to create a function for each of the
 * strings in 'tags'. This will allow us to use s-expression like syntax
 * to create HTML.
 */
function makeTagFunc(tagName) {
  return function (attrs /* rest... */) {
    var startChildren = 0;
    var response = "";

    // write the start tag and attributes
    response += "<" + tagName;
    // if attr is an object, write attributes
    if (attrs && typeof attrs == "object") {
      startChildren = 1;

      for (let key in attrs) {
        const value = attrs[key];
        var val = "" + value;
        response += " " + key + '="' + val.replace('"', "&quot;") + '"';
      }
    }
    response += ">";

    // iterate through the rest of the args
    for (var i = startChildren; i < arguments.length; i++) {
      if (typeof arguments[i] == "function") {
        response += arguments[i]();
      } else {
        response += arguments[i];
      }
    }

    // write the close tag
    response += "</" + tagName + ">\n";
    return response;
  };
}

function makeTags() {
  // map our global HTML generation functions
  for (let tag of tags) {
    this[tag] = makeTagFunc(tag.toLowerCase());
  }
}

/**
 * Creates a generator that iterates over the contents of
 * an nsIFile directory.
 */
function* dirIter(dir) {
  var en = dir.directoryEntries;
  while (en.hasMoreElements()) {
    yield en.nextFile;
  }
}

/**
 * Builds an optionally nested object containing links to the
 * files and directories within dir.
 */
function list(requestPath, directory, recurse) {
  var count = 0;
  var path = requestPath;
  if (path.charAt(path.length - 1) != "/") {
    path += "/";
  }

  var dir = directory.QueryInterface(Ci.nsIFile);
  var links = {};

  // The SimpleTest directory is hidden
  let files = [];
  for (let file of dirIter(dir)) {
    if (file.exists() && !file.path.includes("SimpleTest")) {
      files.push(file);
    }
  }

  // Sort files by name, so that tests can be run in a pre-defined order inside
  // a given directory (see bug 384823)
  function leafNameComparator(first, second) {
    if (first.leafName < second.leafName) {
      return -1;
    }
    if (first.leafName > second.leafName) {
      return 1;
    }
    return 0;
  }
  files.sort(leafNameComparator);

  count = files.length;
  for (let file of files) {
    var key = path + file.leafName;
    var childCount = 0;
    if (file.isDirectory()) {
      key += "/";
    }
    if (recurse && file.isDirectory()) {
      [links[key], childCount] = list(key, file, recurse);
      count += childCount;
    } else if (file.leafName.charAt(0) != ".") {
      links[key] = { test: { url: key, expected: "pass" } };
    }
  }

  return [links, count];
}

/**
 * Heuristic function that determines whether a given path
 * is a test case to be executed in the harness, or just
 * a supporting file.
 */
function isTest(filename, pattern) {
  if (pattern) {
    return pattern.test(filename);
  }

  // File name is a URL style path to a test file, make sure that we check for
  // tests that start with the appropriate prefix.
  var testPrefix = typeof _TEST_PREFIX == "string" ? _TEST_PREFIX : "test_";
  var testPattern = new RegExp("^" + testPrefix);

  var pathPieces = filename.split("/");

  return (
    testPattern.test(pathPieces[pathPieces.length - 1]) &&
    !filename.includes(".js") &&
    !filename.includes(".css") &&
    !/\^headers\^$/.test(filename)
  );
}

/**
 * Transform nested hashtables of paths to nested HTML lists.
 */
function linksToListItems(links) {
  var response = "";
  var children = "";
  for (let link in links) {
    const value = links[link];
    var classVal =
      !isTest(link) && !(value instanceof Object)
        ? "non-test invisible"
        : "test";
    if (value instanceof Object) {
      children = UL({ class: "testdir" }, linksToListItems(value));
    } else {
      children = "";
    }

    var bug_title = link.match(/test_bug\S+/);
    var bug_num = null;
    if (bug_title != null) {
      bug_num = bug_title[0].match(/\d+/);
    }

    if (bug_title == null || bug_num == null) {
      response += LI({ class: classVal }, A({ href: link }, link), children);
    } else {
      var bug_url = "https://bugzilla.mozilla.org/show_bug.cgi?id=" + bug_num;
      response += LI(
        { class: classVal },
        A({ href: link }, link),
        " - ",
        A({ href: bug_url }, "Bug " + bug_num),
        children
      );
    }
  }
  return response;
}

/**
 * Transform nested hashtables of paths to a flat table rows.
 */
function linksToTableRows(links, recursionLevel) {
  var response = "";
  for (let link in links) {
    const value = links[link];
    var classVal =
      !isTest(link) && value instanceof Object && "test" in value
        ? "non-test invisible"
        : "";

    var spacer = "padding-left: " + 10 * recursionLevel + "px";

    if (value instanceof Object && !("test" in value)) {
      response += TR(
        { class: "dir", id: "tr-" + link },
        TD({ colspan: "3" }, "&#160;"),
        TD({ style: spacer }, A({ href: link }, link))
      );
      response += linksToTableRows(value, recursionLevel + 1);
    } else {
      var bug_title = link.match(/test_bug\S+/);
      var bug_num = null;
      if (bug_title != null) {
        bug_num = bug_title[0].match(/\d+/);
      }
      if (bug_title == null || bug_num == null) {
        response += TR(
          { class: classVal, id: "tr-" + link },
          TD("0"),
          TD("0"),
          TD("0"),
          TD({ style: spacer }, A({ href: link }, link))
        );
      } else {
        var bug_url = "https://bugzilla.mozilla.org/show_bug.cgi?id=" + bug_num;
        response += TR(
          { class: classVal, id: "tr-" + link },
          TD("0"),
          TD("0"),
          TD("0"),
          TD(
            { style: spacer },
            A({ href: link }, link),
            " - ",
            A({ href: bug_url }, "Bug " + bug_num)
          )
        );
      }
    }
  }
  return response;
}

function arrayOfTestFiles(linkArray, fileArray, testPattern) {
  for (let link in linkArray) {
    const value = linkArray[link];
    if (value instanceof Object && !("test" in value)) {
      arrayOfTestFiles(value, fileArray, testPattern);
    } else if (isTest(link, testPattern) && value instanceof Object) {
      fileArray.push(value.test);
    }
  }
}
/**
 * Produce a flat array of test file paths to be executed in the harness.
 */
function jsonArrayOfTestFiles(links) {
  var testFiles = [];
  arrayOfTestFiles(links, testFiles);
  testFiles = testFiles.map(function (file) {
    return '"' + file.url + '"';
  });

  return "[" + testFiles.join(",\n") + "]";
}

/**
 * Produce a normal directory listing.
 */
function regularListing(metadata, response) {
  var [links] = list(metadata.path, metadata.getProperty("directory"), false);
  response.write(
    "<!DOCTYPE html>\n" +
      HTML(
        HEAD(TITLE("mochitest index ", metadata.path)),
        BODY(BR(), A({ href: ".." }, "Up a level"), UL(linksToListItems(links)))
      )
  );
}
