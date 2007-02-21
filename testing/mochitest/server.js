/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MozJSHTTP code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Sayre <sayrer@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Note that the server script itself already defines Cc, Ci, and Cr for us,
// and because they're constants it's not safe to redefine them.  Scope leakage
// sucks.

const SERVER_PORT = 8888;
var server; // for use in the shutdown handler, if necessary

//
// HTML GENERATION
//
var tags = ['A', 'ABBR', 'ACRONYM', 'ADDRESS', 'APPLET', 'AREA', 'B', 'BASE',
            'BASEFONT', 'BDO', 'BIG', 'BLOCKQUOTE', 'BODY', 'BR', 'BUTTON',
            'CAPTION', 'CENTER', 'CITE', 'CODE', 'COL', 'COLGROUP', 'DD',
            'DEL', 'DFN', 'DIR', 'DIV', 'DL', 'DT', 'EM', 'FIELDSET', 'FONT',
            'FORM', 'FRAME', 'FRAMESET', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6',
            'HEAD', 'HR', 'HTML', 'I', 'IFRAME', 'IMG', 'INPUT', 'INS',
            'ISINDEX', 'KBD', 'LABEL', 'LEGEND', 'LI', 'LINK', 'MAP', 'MENU',
            'META', 'NOFRAMES', 'NOSCRIPT', 'OBJECT', 'OL', 'OPTGROUP',
            'OPTION', 'P', 'PARAM', 'PRE', 'Q', 'S', 'SAMP', 'SCRIPT',
            'SELECT', 'SMALL', 'SPAN', 'STRIKE', 'STRONG', 'STYLE', 'SUB',
            'SUP', 'TABLE', 'TBODY', 'TD', 'TEXTAREA', 'TFOOT', 'TH', 'THEAD',
            'TITLE', 'TR', 'TT', 'U', 'UL', 'VAR'];

/**
 * Below, we'll use makeTagFunc to create a function for each of the
 * strings in 'tags'. This will allow us to use s-expression like syntax
 * to create HTML.
 */
function makeTagFunc(tagName)
{
  return function (attrs /* rest... */)
  {
    var startChildren = 0;
    var response = "";
    
    // write the start tag and attributes
    response += "<" + tagName;
    // if attr is an object, write attributes
    if (attrs && typeof attrs == 'object') {
      startChildren = 1;
      for (var [key,value] in attrs) {
        var val = "" + value;
        response += " " + key + '="' + val.replace('"','&quot;') + '"';
      }
    }
    response += ">";
    
    // iterate through the rest of the args
    for (var i = startChildren; i < arguments.length; i++) {
      if (typeof arguments[i] == 'function') {
        response += arguments[i]();
      } else {
        response += arguments[i];
      }
    }

    // write the close tag
    response += "</" + tagName + ">\n";
    return response;
  }
}

function makeTags() {
  // map our global HTML generation functions
  for each(var tag in tags) {
      this[tag] = makeTagFunc(tag);
  }
}

// only run the "main" section if httpd.js was loaded ahead of us
if (this["nsHttpServer"]) {
  //
  // SCRIPT CODE
  //
  runServer();

  // We can only have gotten here if CLOSE_WHEN_DONE was specified and the
  // /server/shutdown path was requested.  We can shut down the xpcshell now
  // that all testing requests have been served.
  quit(0);
}

var serverBasePath;

//
// SERVER SETUP
//
function runServer()
{
  serverBasePath = Cc["@mozilla.org/file/local;1"]
                     .createInstance(Ci.nsILocalFile);
  var procDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties).get("CurProcD", Ci.nsIFile);
  serverBasePath.initWithPath(procDir.parent.parent.path);
  serverBasePath.append("_tests");
  serverBasePath.append("testing");
  serverBasePath.append("mochitest");
  server = new nsHttpServer();
  server.registerDirectory("/", serverBasePath);

  if (environment["CLOSE_WHEN_DONE"])
    server.registerPathHandler("/server/shutdown", serverShutdown);

  server.setIndexHandler(defaultDirHandler);
  server.start(SERVER_PORT);
  
  // touch a file in the profile directory to indicate we're alive
  var foStream = Cc["@mozilla.org/network/file-output-stream;1"]
                   .createInstance(Ci.nsIFileOutputStream);
  var serverAlive = Cc["@mozilla.org/file/local;1"]
                      .createInstance(Ci.nsILocalFile);
  serverAlive.initWithFile(serverBasePath);
  serverAlive.append("mochitesttestingprofile");

  // If we're running outside of the test harness, there might
  // not be a test profile directory present
  if (serverAlive.exists()) {
    serverAlive.append("server_alive.txt");
    foStream.init(serverAlive,
                  0x02 | 0x08 | 0x20, 0664, 0); // write, create, truncate
    data = "It's alive!";
    foStream.write(data, data.length);
    foStream.close();
  }

  makeTags();

  //
  // The following is threading magic to spin an event loop -- this has to
  // happen manually in xpcshell for the server to actually work.
  //
  var thread = Cc["@mozilla.org/thread-manager;1"]
                 .getService()
                 .currentThread;
  while (!server.isStopped())
    thread.processNextEvent(true);

  // Server stopped by /server/shutdown handler -- go through pending events
  // and return.

  // get rid of any pending requests
  while (thread.hasPendingEvents())
    thread.processNextEvent(true);
}

// PATH HANDLERS

// /server/shutdown
function serverShutdown(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
  response.setHeader("Content-type", "text/plain");

  var body = "Server shut down.";
  response.bodyOutputStream.write(body, body.length);

  // Note: this doesn't disrupt the current request.
  server.stop();
}

//
// DIRECTORY LISTINGS
//

/**
 * Creates a generator that iterates over the contents of
 * an nsIFile directory.
 */
function dirIter(dir)
{
  var enum = dir.directoryEntries;
  while (enum.hasMoreElements()) {
    var file = enum.getNext();
    yield file.QueryInterface(Ci.nsILocalFile);
  }
}

/**
 * Builds an optionally nested object containing links to the
 * files and directories within dir.
 */
function list(requestPath, directory, recurse)
{
  var count = 0;
  var path = requestPath;
  if (path.charAt(path.length - 1) != "/") {
    path += "/";
  }

  var dir = directory.QueryInterface(Ci.nsIFile);
  var links = {};
  
  // The SimpleTest directory is hidden
  var files = [file for (file in dirIter(dir))
               if (file.path.indexOf("SimpleTest") == -1)];
  
  count = files.length;
  for each (var file in files) {
    var key = requestPath + file.leafName;
    var childCount = 0;
    if (file.isDirectory()) {
      key += "/";
    }
    if (recurse && file.isDirectory()) {
      [links[key], childCount] = list(key, file, recurse);
      count += childCount;
    } else {
      links[key] = true;
    }
  }

  return [links, count];
}

/**
 * Heuristic function that determines whether a given path
 * is a test case to be executed in the harness, or just
 * a supporting file.
 */
function isTest(filename)
{
  return  (filename.indexOf("test_") > -1 &&
           filename.indexOf(".js") == -1 &&
           filename.indexOf(".css") == -1);
}

/**
 * Transform nested hashtables of paths to nested HTML lists.
 */
function linksToListItems(links)
{
  var response = "";
  var children = "";
  for (var [link, value] in links) {
    var classVal = (!isTest(link) && !(value instanceof Object))
      ? "non-test invisible"
      : "test";
    if (value instanceof Object) {
      children = UL({class: "testdir"}, linksToListItems(value)); 
    } else {
      children = "";
    }
    response += LI({class: classVal}, A({href: link}, link), children);

  }
  return response;
}

/**
 * Transform nested hashtables of paths to a flat table rows.
 */
function linksToTableRows(links)
{
  var response = "";
  for (var [link, value] in links) {
    var classVal = (!isTest(link) && !(value instanceof Object))
      ? "non-test invisible"
      : "";
    if (value instanceof Object) {
      response += TR({class: "dir", id: "tr-" + link },
                     TD({colspan: "3"},"&#160;"));
      response += linksToTableRows(value);
    } else {
      response += TR({class: classVal, id: "tr-" + link},
                     TD("0"), TD("0"), TD("0"));
    }
  }
  return response;
}

/**
 * Produce a flat array of test file paths to be executed in the harness.
 */
function jsonArrayOfTestFiles(links)
{
  var testFiles = [];
  function arrayOfTestFiles(linkArray) {
    for (var [link, value] in linkArray) {
      if (value instanceof Object) {
        arrayOfTestFiles(value);
      } else {
        testFiles.push(link)
      }
    }
  }
  arrayOfTestFiles(links);
  var testFiles = ['"' + file + '"' for each(file in testFiles)
                   if (isTest(file))];
  return "[" + testFiles.join(",\n") + "]";
}

/**
 * Produce a normal directory listing.
 */
function regularListing(metadata, response)
{
  var [links, count] = list(metadata.path,
                            metadata.getProperty("directory"),
                            false);
  response.write(
    HTML(
      HEAD(
        TITLE("mochitest index ", metadata.path)
      ),
      BODY(
        BR(),
        A({href: ".."}, "Up a level"),
        UL(linksToListItems(links))
      )
    )
  );
}

/**
 * Produce a test harness page containing all the test cases
 * below it, recursively.
 */
function testListing(metadata, response)
{
  var [links, count] = list(metadata.path,
                            metadata.getProperty("directory"),
                            true);
  dumpn("count: " + count);
  var tests = jsonArrayOfTestFiles(links);
  response.write(
    HTML(
      HEAD(
        TITLE("MochiTest | ", metadata.path),
        LINK({rel: "stylesheet",
              type: "text/css", href: "/static/harness.css"}
        ),
        SCRIPT({type: "text/javascript", src: "/MochiKit/packed.js"}),
        SCRIPT({type: "text/javascript",
                 src: "/tests/SimpleTest/TestRunner.js"}),
        SCRIPT({type: "text/javascript",
                 src: "/tests/SimpleTest/MozillaFileLogger.js"}),
        SCRIPT({type: "text/javascript",
                 src: "/tests/SimpleTest/quit.js"}),
        SCRIPT({type: "text/javascript",
                 src: "/tests/SimpleTest/setup.js"}),
        SCRIPT({type: "text/javascript"},
               "connect(window, 'onload', hookup); gTestList=" + tests + ";"
        )
      ),
      BODY(
        DIV({class: "container"},
          H2("--> ", A({href: "#", id: "runtests"}, "Run Tests"), " <--"),
            P({style: "float: right;"},
            SMALL(
              "Based on the ",
              A({href:"http://www.mochikit.com/"}, "MochiKit"),
              " unit tests."
            )
          ),
          DIV({class: "status"},
            H1({id: "indicator"}, "Status"),
            H2({id: "pass"}, "Passed: ", SPAN({id: "pass-count"},"0")),
            H2({id: "fail"}, "Failed: ", SPAN({id: "fail-count"},"0")),
            H2({id: "fail"}, "Todo: ", SPAN({id: "todo-count"},"0"))
          ),
          DIV({class: "clear"}),
          DIV({id: "current-test"},
            B("Currently Executing: ",
              SPAN({id: "current-test-path"}, "_")
            )
          ),
          DIV({class: "clear"}),
          DIV({class: "frameholder"},
            IFRAME({scrolling: "no", id: "testframe", width: "500"})
          ),
          DIV({class: "clear"}),
          DIV({class: "toggle"},
            A({href: "#", id: "toggleNonTests"}, "Show Non-Tests"),
            BR()
          ),
    
          TABLE({cellpadding: 0, cellspacing: 0, id: "test-table"},
            TR(TD("Passed"), TD("Failed"), TD("Todo"), 
                TD({rowspan: count+1},
                   UL({class: "top"},
                      LI(B("Test Files")),        
                      linksToListItems(links)
                      )
                )
            ),
            linksToTableRows(links)
          ),
          DIV({class: "clear"})
        )
      )
    )
  );
}

/**
 * Respond to requests that match a file system directory.
 * Under the tests/ directory, return a test harness page.
 */
function defaultDirHandler(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
  response.setHeader("Content-type", "text/html");
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
