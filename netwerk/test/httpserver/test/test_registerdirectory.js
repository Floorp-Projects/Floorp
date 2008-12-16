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
 * The Original Code is httpd.js code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// tests the registerDirectory API

const BASE = "http://localhost:4444";

var tests = [];
var test;


function nocache(ch)
{
  ch.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE; // important!
}

function notFound(ch)
{
  do_check_eq(ch.responseStatus, 404);
  do_check_false(ch.requestSucceeded);
}

function checkOverride(ch)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.responseStatusText, "OK");
  do_check_true(ch.requestSucceeded);
  do_check_eq(ch.getResponseHeader("Override-Succeeded"), "yes");
}

function check200(ch)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.responseStatusText, "OK");
}

function checkFile(ch)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_true(ch.requestSucceeded);

  var actualFile = serverBasePath.clone();
  actualFile.append("test_registerdirectory.js");
  do_check_eq(ch.getResponseHeader("Content-Length"),
              actualFile.fileSize.toString());
}


/***********************
 * without a base path *
 ***********************/

test = new Test(BASE + "/test_registerdirectory.js",
                nocache, notFound, null),
tests.push(test);


/********************
 * with a base path *
 ********************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  serverBasePath = testsDirectory.clone();
                  srv.registerDirectory("/", serverBasePath);
                },
                checkFile,
                null);
tests.push(test);


/*****************************
 * without a base path again *
 *****************************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  serverBasePath = null;
                  srv.registerDirectory("/", serverBasePath);
                },
                notFound,
                null);
tests.push(test);


/***************************
 * registered path handler *
 ***************************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  srv.registerPathHandler("/test_registerdirectory.js",
                                          override_test_registerdirectory);
                },
                checkOverride,
                null);
tests.push(test);


/************************
 * removed path handler *
 ************************/

test = new Test(BASE + "/test_registerdirectory.js",
                function init_registerDirectory6(ch)
                {
                  nocache(ch);
                  srv.registerPathHandler("/test_registerdirectory.js", null);
                },
                notFound,
                null);
tests.push(test);


/********************
 * with a base path *
 ********************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);

                  // set the base path again
                  serverBasePath = testsDirectory.clone();
                  srv.registerDirectory("/", serverBasePath);
                },
                checkFile,
                null);
tests.push(test);


/*************************
 * ...and a path handler *
 *************************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  srv.registerPathHandler("/test_registerdirectory.js",
                                          override_test_registerdirectory);
                },
                checkOverride,
                null);
tests.push(test);


/************************
 * removed base handler *
 ************************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  serverBasePath = null;
                  srv.registerDirectory("/", serverBasePath);
                },
                checkOverride,
                null);
tests.push(test);


/************************
 * removed path handler *
 ************************/

test = new Test(BASE + "/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  srv.registerPathHandler("/test_registerdirectory.js", null);
                },
                notFound,
                null);
tests.push(test);


/*************************
 * mapping set up, works *
 *************************/

test = new Test(BASE + "/foo/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  serverBasePath = testsDirectory.clone();
                  srv.registerDirectory("/foo/", serverBasePath);
                },
                check200,
                null);
tests.push(test);


/*********************
 * no mapping, fails *
 *********************/

test = new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
                nocache,
                notFound,
                null);
tests.push(test);


/******************
 * mapping, works *
 ******************/

test = new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  srv.registerDirectory("/foo/test_registerdirectory.js/",
                                        serverBasePath);
                },
                checkFile,
                null);
tests.push(test);


/************************************
 * two mappings set up, still works *
 ************************************/

test = new Test(BASE + "/foo/test_registerdirectory.js",
                nocache, checkFile, null);
tests.push(test);


/**************************
 * remove topmost mapping *
 **************************/

test = new Test(BASE + "/foo/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  srv.registerDirectory("/foo/", null);
                },
                notFound,
                null);
tests.push(test);


/**************************************
 * lower mapping still present, works *
 **************************************/

test = new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
                nocache, checkFile, null);
tests.push(test);


/*******************
 * mapping removed *
 *******************/

test = new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
                function(ch)
                {
                  nocache(ch);
                  srv.registerDirectory("/foo/test_registerdirectory.js/", null);
                },
                notFound,
                null);
tests.push(test);



var srv;
var serverBasePath;
var testsDirectory;

function run_test()
{
  testsDirectory = do_get_file("netwerk/test/httpserver/test/");

  srv = createServer();
  srv.start(4444);

  runHttpTests(tests, function() { srv.stop(); });
}


// PATH HANDLERS

// override of /test_registerdirectory.js
function override_test_registerdirectory(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
  response.setHeader("Override-Succeeded", "yes", false);

  var body = "success!";
  response.bodyOutputStream.write(body, body.length);
}
