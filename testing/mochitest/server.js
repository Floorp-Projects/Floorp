/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We expect these to be defined in the global scope by runtest.py.
/* global __LOCATION__, _PROFILE_PATH, _SERVER_PORT, _SERVER_ADDR, _DISPLAY_RESULTS,
          _TEST_PREFIX */
// Defined by xpcshell
/* global quit */

/* import-globals-from ../../netwerk/test/httpserver/httpd.js */
/* eslint-disable mozilla/use-chromeutils-generateqi */

// Disable automatic network detection, so tests work correctly when
// not connected to a network.
// eslint-disable-next-line mozilla/use-services
var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
ios.manageOfflineStatus = false;
ios.offline = false;

var server; // for use in the shutdown handler, if necessary

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
  return function(attrs /* rest... */) {
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

var _quitting = false;

/** Quit when all activity has completed. */
function serverStopped() {
  _quitting = true;
}

// only run the "main" section if httpd.js was loaded ahead of us
if (this.nsHttpServer) {
  //
  // SCRIPT CODE
  //
  runServer();

  // We can only have gotten here if the /server/shutdown path was requested.
  if (_quitting) {
    dumpn("HTTP server stopped, all pending requests complete");
    quit(0);
  }

  // Impossible as the stop callback should have been called, but to be safe...
  dumpn("TEST-UNEXPECTED-FAIL | failure to correctly shut down HTTP server");
  quit(1);
}

var serverBasePath;
var displayResults = true;

var gServerAddress;
var SERVER_PORT;

//
// SERVER SETUP
//
function runServer() {
  serverBasePath = __LOCATION__.parent;
  server = createMochitestServer(serverBasePath);

  // verify server address
  // if a.b.c.d or 'localhost'
  if (typeof _SERVER_ADDR != "undefined") {
    if (_SERVER_ADDR == "localhost") {
      gServerAddress = _SERVER_ADDR;
    } else {
      var quads = _SERVER_ADDR.split(".");
      if (quads.length == 4) {
        var invalid = false;
        for (var i = 0; i < 4; i++) {
          if (quads[i] < 0 || quads[i] > 255) {
            invalid = true;
          }
        }
        if (!invalid) {
          gServerAddress = _SERVER_ADDR;
        } else {
          throw new Error(
            "invalid _SERVER_ADDR, please specify a valid IP Address"
          );
        }
      }
    }
  } else {
    throw new Error(
      "please define _SERVER_ADDR (as an ip address) before running server.js"
    );
  }

  if (typeof _SERVER_PORT != "undefined") {
    if (parseInt(_SERVER_PORT) > 0 && parseInt(_SERVER_PORT) < 65536) {
      SERVER_PORT = _SERVER_PORT;
    }
  } else {
    throw new Error(
      "please define _SERVER_PORT (as a port number) before running server.js"
    );
  }

  // If DISPLAY_RESULTS is not specified, it defaults to true
  if (typeof _DISPLAY_RESULTS != "undefined") {
    displayResults = _DISPLAY_RESULTS;
  }

  server._start(SERVER_PORT, gServerAddress);

  // touch a file in the profile directory to indicate we're alive
  var foStream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  var serverAlive = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);

  if (typeof _PROFILE_PATH == "undefined") {
    serverAlive.initWithFile(serverBasePath);
    serverAlive.append("mochitesttestingprofile");
  } else {
    serverAlive.initWithPath(_PROFILE_PATH);
  }

  // Create a file to inform the harness that the server is ready
  if (serverAlive.exists()) {
    serverAlive.append("server_alive.txt");
    foStream.init(serverAlive, 0x02 | 0x08 | 0x20, 436, 0); // write, create, truncate
    var data = "It's alive!";
    foStream.write(data, data.length);
    foStream.close();
  } else {
    throw new Error(
      "Failed to create server_alive.txt because " +
        serverAlive.path +
        " could not be found."
    );
  }

  makeTags();

  //
  // The following is threading magic to spin an event loop -- this has to
  // happen manually in xpcshell for the server to actually work.
  //
  var thread = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;
  while (!server.isStopped()) {
    thread.processNextEvent(true);
  }

  // Server stopped by /server/shutdown handler -- go through pending events
  // and return.

  // get rid of any pending requests
  while (thread.hasPendingEvents()) {
    thread.processNextEvent(true);
  }
}

/** Creates and returns an HTTP server configured to serve Mochitests. */
function createMochitestServer(serverBasePath) {
  var server = new nsHttpServer();

  server.registerDirectory("/", serverBasePath);
  server.registerPathHandler("/server/shutdown", serverShutdown);
  server.registerPathHandler("/server/debug", serverDebug);
  server.registerContentType("sjs", "sjs"); // .sjs == CGI-like functionality
  server.registerContentType("jar", "application/x-jar");
  server.registerContentType("ogg", "application/ogg");
  server.registerContentType("pdf", "application/pdf");
  server.registerContentType("ogv", "video/ogg");
  server.registerContentType("oga", "audio/ogg");
  server.registerContentType("opus", "audio/ogg; codecs=opus");
  server.registerContentType("dat", "text/plain; charset=utf-8");
  server.registerContentType("frag", "text/plain"); // .frag == WebGL fragment shader
  server.registerContentType("vert", "text/plain"); // .vert == WebGL vertex shader
  server.registerContentType("wasm", "application/wasm");
  server.setIndexHandler(defaultDirHandler);

  var serverRoot = {
    getFile: function getFile(path) {
      var file = serverBasePath.clone().QueryInterface(Ci.nsIFile);
      path.split("/").forEach(function(p) {
        file.appendRelativePath(p);
      });
      return file;
    },
    QueryInterface(aIID) {
      return this;
    },
  };

  server.setObjectState("SERVER_ROOT", serverRoot);

  processLocations(server);

  return server;
}

/**
 * Notifies the HTTP server about all the locations at which it might receive
 * requests, so that it can properly respond to requests on any of the hosts it
 * serves.
 */
function processLocations(server) {
  var serverLocations = serverBasePath.clone();
  serverLocations.append("server-locations.txt");

  const PR_RDONLY = 0x01;
  var fis = new FileInputStream(
    serverLocations,
    PR_RDONLY,
    292 /* 0444 */,
    Ci.nsIFileInputStream.CLOSE_ON_EOF
  );

  var lis = new ConverterInputStream(fis, "UTF-8", 1024, 0x0);
  lis.QueryInterface(Ci.nsIUnicharLineInputStream);

  const LINE_REGEXP = new RegExp(
    "^([a-z][-a-z0-9+.]*)" +
      "://" +
      "(" +
      "\\d+\\.\\d+\\.\\d+\\.\\d+" +
      "|" +
      "(?:[a-z0-9](?:[-a-z0-9]*[a-z0-9])?\\.)*" +
      "[a-z](?:[-a-z0-9]*[a-z0-9])?" +
      ")" +
      ":" +
      "(\\d+)" +
      "(?:" +
      "\\s+" +
      "(\\S+(?:,\\S+)*)" +
      ")?$"
  );

  var line = {};
  var lineno = 0;
  var seenPrimary = false;
  do {
    var more = lis.readLine(line);
    lineno++;

    var lineValue = line.value;
    if (lineValue.charAt(0) == "#" || lineValue == "") {
      continue;
    }

    var match = LINE_REGEXP.exec(lineValue);
    if (!match) {
      throw new Error("Syntax error in server-locations.txt, line " + lineno);
    }

    var [, scheme, host, port, options] = match;
    if (options) {
      if (options.split(",").includes("primary")) {
        if (seenPrimary) {
          throw new Error(
            "Multiple primary locations in server-locations.txt, " +
              "line " +
              lineno
          );
        }

        server.identity.setPrimary(scheme, host, port);
        seenPrimary = true;
        continue;
      }
    }

    server.identity.add(scheme, host, port);
  } while (more);
}

// PATH HANDLERS

// /server/shutdown
function serverShutdown(metadata, response) {
  response.setStatusLine("1.1", 200, "OK");
  response.setHeader("Content-type", "text/plain", false);

  var body = "Server shut down.";
  response.bodyOutputStream.write(body, body.length);

  dumpn("Server shutting down now...");
  server.stop(serverStopped);
}

// /server/debug?[012]
function serverDebug(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 400, "Bad debugging level");
  if (metadata.queryString.length !== 1) {
    return;
  }

  var mode;
  if (metadata.queryString === "0") {
    // do this now so it gets logged with the old mode
    dumpn("Server debug logs disabled.");
    DEBUG = false;
    DEBUG_TIMESTAMP = false;
    mode = "disabled";
  } else if (metadata.queryString === "1") {
    DEBUG = true;
    DEBUG_TIMESTAMP = false;
    mode = "enabled";
  } else if (metadata.queryString === "2") {
    DEBUG = true;
    DEBUG_TIMESTAMP = true;
    mode = "enabled, with timestamps";
  } else {
    return;
  }

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-type", "text/plain", false);
  var body = "Server debug logs " + mode + ".";
  response.bodyOutputStream.write(body, body.length);
  dumpn(body);
}

//
// DIRECTORY LISTINGS
//

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
  testFiles = testFiles.map(function(file) {
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
    HTML(
      HEAD(TITLE("mochitest index ", metadata.path)),
      BODY(BR(), A({ href: ".." }, "Up a level"), UL(linksToListItems(links)))
    )
  );
}

/**
 * Read a manifestFile located at the root of the server's directory and turn
 * it into an object for creating a table of clickable links for each test.
 */
function convertManifestToTestLinks(root, manifest) {
  const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

  var manifestFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  manifestFile.initWithFile(serverBasePath);
  manifestFile.append(manifest);

  var manifestStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  manifestStream.init(manifestFile, -1, 0, 0);

  var manifestObj = JSON.parse(
    NetUtil.readInputStreamToString(manifestStream, manifestStream.available())
  );
  var paths = manifestObj.tests;
  var pathPrefix = "/" + root + "/";
  return [
    paths.reduce(function(t, p) {
      t[pathPrefix + p.path] = true;
      return t;
    }, {}),
    paths.length,
  ];
}

/**
 * Produce a test harness page containing all the test cases
 * below it, recursively.
 */
function testListing(metadata, response) {
  var links = {};
  var count = 0;
  if (!metadata.queryString.includes("manifestFile")) {
    [links, count] = list(
      metadata.path,
      metadata.getProperty("directory"),
      true
    );
  } else if (typeof Components != undefined) {
    var manifest = metadata.queryString.match(/manifestFile=([^&]+)/)[1];

    [links, count] = convertManifestToTestLinks(
      metadata.path.split("/")[1],
      manifest
    );
  }

  var table_class =
    metadata.queryString.indexOf("hideResultsTable=1") > -1 ? "invisible" : "";

  let testname =
    metadata.queryString.indexOf("testname=") > -1
      ? metadata.queryString.match(/testname=([^&]+)/)[1]
      : "";

  dumpn("count: " + count);
  var tests = testname ? "['/" + testname + "']" : jsonArrayOfTestFiles(links);
  response.write(
    HTML(
      HEAD(
        TITLE("MochiTest | ", metadata.path),
        LINK({
          rel: "stylesheet",
          type: "text/css",
          href: "/static/harness.css",
        }),
        SCRIPT({
          type: "text/javascript",
          src: "/tests/SimpleTest/StructuredLog.jsm",
        }),
        SCRIPT({
          type: "text/javascript",
          src: "/tests/SimpleTest/LogController.js",
        }),
        SCRIPT({
          type: "text/javascript",
          src: "/tests/SimpleTest/MemoryStats.js",
        }),
        SCRIPT({
          type: "text/javascript",
          src: "/tests/SimpleTest/TestRunner.js",
        }),
        SCRIPT({
          type: "text/javascript",
          src: "/tests/SimpleTest/MozillaLogger.js",
        }),
        SCRIPT({ type: "text/javascript", src: "/chunkifyTests.js" }),
        SCRIPT({ type: "text/javascript", src: "/manifestLibrary.js" }),
        SCRIPT({ type: "text/javascript", src: "/tests/SimpleTest/setup.js" }),
        SCRIPT(
          { type: "text/javascript" },
          "window.onload =  hookup; gTestList=" + tests + ";"
        )
      ),
      BODY(
        DIV(
          { class: "container" },
          H2("--> ", A({ href: "#", id: "runtests" }, "Run Tests"), " <--"),
          P(
            { style: "float: right;" },
            SMALL(
              "Based on the ",
              A({ href: "http://www.mochikit.com/" }, "MochiKit"),
              " unit tests."
            )
          ),
          DIV(
            { class: "status" },
            H1({ id: "indicator" }, "Status"),
            H2({ id: "pass" }, "Passed: ", SPAN({ id: "pass-count" }, "0")),
            H2({ id: "fail" }, "Failed: ", SPAN({ id: "fail-count" }, "0")),
            H2({ id: "fail" }, "Todo: ", SPAN({ id: "todo-count" }, "0"))
          ),
          DIV({ class: "clear" }),
          DIV(
            { id: "current-test" },
            B("Currently Executing: ", SPAN({ id: "current-test-path" }, "_"))
          ),
          DIV({ class: "clear" }),
          DIV(
            { class: "frameholder" },
            IFRAME({ scrolling: "no", id: "testframe", allowfullscreen: true })
          ),
          DIV({ class: "clear" }),
          DIV(
            { class: "toggle" },
            A({ href: "#", id: "toggleNonTests" }, "Show Non-Tests"),
            BR()
          ),

          displayResults
            ? TABLE(
                {
                  cellpadding: 0,
                  cellspacing: 0,
                  class: table_class,
                  id: "test-table",
                },
                TR(TD("Passed"), TD("Failed"), TD("Todo"), TD("Test Files")),
                linksToTableRows(links, 0)
              )
            : "",
          BR(),
          TABLE({
            cellpadding: 0,
            cellspacing: 0,
            border: 1,
            bordercolor: "red",
            id: "fail-table",
          }),

          DIV({ class: "clear" })
        )
      )
    )
  );
}

/**
 * Respond to requests that match a file system directory.
 * Under the tests/ directory, return a test harness page.
 */
function defaultDirHandler(metadata, response) {
  response.setStatusLine("1.1", 200, "OK");
  response.setHeader("Content-type", "text/html;charset=utf-8", false);
  try {
    if (metadata.path.indexOf("/tests") != 0) {
      regularListing(metadata, response);
    } else {
      testListing(metadata, response);
    }
  } catch (ex) {
    response.write(ex);
  }
}
