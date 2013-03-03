/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// checks for correct output with the default index handler, mostly to do
// escaping checks -- highly dependent on the default index handler output
// format

var srv, dir, dirEntries;

function run_test()
{
  createTestDirectory();

  srv = createServer();
  srv.registerDirectory("/", dir);

  var nameDir = do_get_file("data/name-scheme/");
  srv.registerDirectory("/bar/", nameDir);

  srv.start(4444);

  function done()
  {
    do_test_pending();
    destroyTestDirectory();
    srv.stop(function() { do_test_finished(); });
  }

  runHttpTests(tests, done);
}

function createTestDirectory()
{
  dir = Cc["@mozilla.org/file/directory_service;1"]
          .getService(Ci.nsIProperties)
          .get("TmpD", Ci.nsIFile);
  dir.append("index_handler_test_" + Math.random());
  dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0744);

  // populate with test directories, files, etc.
  // Files must be in expected order of display on the index page!

  var files = [];

  makeFile("aa_directory", true, dir, files);
  makeFile("Ba_directory", true, dir, files);
  makeFile("bb_directory", true, dir, files);
  makeFile("foo", true, dir, files);
  makeFile("a_file", false, dir, files);
  makeFile("B_file", false, dir, files);
  makeFile("za'z", false, dir, files);
  makeFile("zb&z", false, dir, files);
  makeFile("zc<q", false, dir, files);
  makeFile('zd"q', false, dir, files);
  makeFile("ze%g", false, dir, files);
  makeFile("zf%200h", false, dir, files);
  makeFile("zg>m", false, dir, files);

  dirEntries = [files];

  var subdir = dir.clone();
  subdir.append("foo");

  files = [];

  makeFile("aa_dir", true, subdir, files);
  makeFile("b_dir", true, subdir, files);
  makeFile("AA_file.txt", false, subdir, files);
  makeFile("test.txt", false, subdir, files);

  dirEntries.push(files);
}

function destroyTestDirectory()
{
  dir.remove(true);
}


/*************
 * UTILITIES *
 *************/

/** Verifies data in bytes for the trailing-caret path above. */
function hiddenDataCheck(bytes, uri, path)
{
  var data = String.fromCharCode.apply(null, bytes);

  var parser = Cc["@mozilla.org/xmlextras/domparser;1"]
                 .createInstance(Ci.nsIDOMParser);

  // Note: the index format isn't XML -- it's actually HTML -- but we require
  //       the index format also be valid XML, albeit XML without namespaces,
  //       XML declarations, etc.  Doing this simplifies output checking.
  try
  {
    var doc = parser.parseFromString(data, "application/xml");
  }
  catch (e)
  {
    do_throw("document failed to parse as XML");
  }

  // See all the .QueryInterface()s and .item()s happening here?  That's because
  // xpcshell sucks and doesn't have classinfo, so no automatic interface
  // flattening or array-style access to items in NodeLists.  Suck.

  var body = doc.documentElement.getElementsByTagName("body");
  do_check_eq(body.length, 1);
  body = body.item(0);

  // header
  var header = body.QueryInterface(Ci.nsIDOMElement)
                   .getElementsByTagName("h1");
  do_check_eq(header.length, 1);

  do_check_eq(header.item(0).QueryInterface(Ci.nsIDOMNode).textContent, path);

  // files
  var lst = body.getElementsByTagName("ol");
  do_check_eq(lst.length, 1);
  var items = lst.item(0).QueryInterface(Ci.nsIDOMElement)
                         .getElementsByTagName("li");

  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);

  var top = ios.newURI(uri, null, null);

  // N.B. No ERROR_IF_SEE_THIS.txt^ file!
  var dirEntries = [{name: "file.txt", isDirectory: false},
                    {name: "SHOULD_SEE_THIS.txt^", isDirectory: false}];

  for (var i = 0; i < items.length; i++)
  {
    var link = items.item(i)
                    .childNodes
                    .item(0)
                    .QueryInterface(Ci.nsIDOMNode)
                    .QueryInterface(Ci.nsIDOMElement);
    var f = dirEntries[i];

    var sep = f.isDirectory ? "/" : "";

    do_check_eq(link.textContent, f.name + sep);

    uri = ios.newURI(link.getAttribute("href"), null, top);
    do_check_eq(decodeURIComponent(uri.path), path + f.name + sep);
  }
}

/**
 * Verifies data in bytes (an array of bytes) represents an index page for the
 * given URI and path, which should be a page listing the given directory
 * entries, in order.
 *
 * @param bytes
 *   array of bytes representing the index page's contents
 * @param uri
 *   string which is the URI of the index page
 * @param path
 *   the path portion of uri
 * @param dirEntries
 *   sorted (in the manner the directory entries should be sorted) array of
 *   objects, each of which has a name property (whose value is the file's name,
 *   without / if it's a directory) and an isDirectory property (with expected
 *   value)
 */
function dataCheck(bytes, uri, path, dirEntries)
{
  var data = String.fromCharCode.apply(null, bytes);

  var parser = Cc["@mozilla.org/xmlextras/domparser;1"]
                 .createInstance(Ci.nsIDOMParser);

  // Note: the index format isn't XML -- it's actually HTML -- but we require
  //       the index format also be valid XML, albeit XML without namespaces,
  //       XML declarations, etc.  Doing this simplifies output checking.
  try
  {
    var doc = parser.parseFromString(data, "application/xml");
  }
  catch (e)
  {
    do_throw("document failed to parse as XML");
  }

  // See all the .QueryInterface()s and .item()s happening here?  That's because
  // xpcshell sucks and doesn't have classinfo, so no automatic interface
  // flattening or array-style access to items in NodeLists.  Suck.

  var body = doc.documentElement.getElementsByTagName("body");
  do_check_eq(body.length, 1);
  body = body.item(0);

  // header
  var header = body.QueryInterface(Ci.nsIDOMElement)
                   .getElementsByTagName("h1");
  do_check_eq(header.length, 1);

  do_check_eq(header.item(0).QueryInterface(Ci.nsIDOMNode).textContent, path);

  // files
  var lst = body.getElementsByTagName("ol");
  do_check_eq(lst.length, 1);
  var items = lst.item(0).QueryInterface(Ci.nsIDOMElement)
                         .getElementsByTagName("li");

  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);

  var dirURI = ios.newURI(uri, null, null);

  for (var i = 0; i < items.length; i++)
  {
    var link = items.item(i)
                    .childNodes
                    .item(0)
                    .QueryInterface(Ci.nsIDOMNode)
                    .QueryInterface(Ci.nsIDOMElement);
    var f = dirEntries[i];

    var sep = f.isDirectory ? "/" : "";

    do_check_eq(link.textContent, f.name + sep);

    uri = ios.newURI(link.getAttribute("href"), null, top);
    do_check_eq(decodeURIComponent(uri.path), path + f.name + sep);
  }
}

/**
 * Create a file/directory with the given name underneath parentDir, and
 * append an object with name/isDirectory properties to lst corresponding
 * to it if the file/directory could be created.
 */
function makeFile(name, isDirectory, parentDir, lst)
{
  var type = Ci.nsIFile[isDirectory ? "DIRECTORY_TYPE" : "NORMAL_FILE_TYPE"];
  var file = parentDir.clone();

  try
  {
    file.append(name);
    file.create(type, 0755);
    lst.push({name: name, isDirectory: isDirectory});
  }
  catch (e) { /* OS probably doesn't like file name, skip */ }
}

/*********
 * TESTS *
 *********/

var tests = [];
var test;

// check top-level directory listing
test = new Test("http://localhost:4444/",
                null, start, stopRootDirectory),
tests.push(test);
function start(ch)
{
  do_check_eq(ch.getResponseHeader("Content-Type"), "text/html;charset=utf-8");
}
function stopRootDirectory(ch, cx, status, data)
{
  dataCheck(data, "http://localhost:4444/", "/", dirEntries[0]);
}


// check non-top-level, too
test = new Test("http://localhost:4444/foo/",
                null, start, stopFooDirectory),
tests.push(test);
function stopFooDirectory(ch, cx, status, data)
{
  dataCheck(data, "http://localhost:4444/foo/", "/foo/", dirEntries[1]);
}


// trailing-caret leaf with hidden files
test = new Test("http://localhost:4444/bar/folder^/",
                null, start, stopTrailingCaretDirectory),
tests.push(test);
function stopTrailingCaretDirectory(ch, cx, status, data)
{
  hiddenDataCheck(data, "http://localhost:4444/bar/folder^/", "/bar/folder^/");
}
