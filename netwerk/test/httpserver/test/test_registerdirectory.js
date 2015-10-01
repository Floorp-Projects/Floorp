/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the registerDirectory API

XPCOMUtils.defineLazyGetter(this, "BASE", function() {
  return "http://localhost:" + srv.identity.primaryPort;
});


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

function checkFile(ch, cx, status, data)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_true(ch.requestSucceeded);

  var actualFile = serverBasePath.clone();
  actualFile.append("test_registerdirectory.js");
  do_check_eq(ch.getResponseHeader("Content-Length"),
              actualFile.fileSize.toString());
  do_check_eq(data.map(v => String.fromCharCode(v)).join(""),
              fileContents(actualFile));
}

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [

/***********************
 * without a base path *
 ***********************/
    new Test(BASE + "/test_registerdirectory.js",
                nocache, notFound, null),

/********************
 * with a base path *
 ********************/
    new Test(BASE + "/test_registerdirectory.js",
           function(ch)
           {
             nocache(ch);
             serverBasePath = testsDirectory.clone();
             srv.registerDirectory("/", serverBasePath);
           },
           null,
           checkFile),

/*****************************
 * without a base path again *
 *****************************/
    new Test(BASE + "/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              serverBasePath = null;
              srv.registerDirectory("/", serverBasePath);
            },
            notFound,
            null),

/***************************
 * registered path handler *
 ***************************/
    new Test(BASE + "/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              srv.registerPathHandler("/test_registerdirectory.js",
                                      override_test_registerdirectory);
            },
            checkOverride,
            null),

/************************
 * removed path handler *
 ************************/
    new Test(BASE + "/test_registerdirectory.js",
            function init_registerDirectory6(ch)
            {
              nocache(ch);
              srv.registerPathHandler("/test_registerdirectory.js", null);
            },
            notFound,
            null),

/********************
 * with a base path *
 ********************/
    new Test(BASE + "/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);

              // set the base path again
              serverBasePath = testsDirectory.clone();
              srv.registerDirectory("/", serverBasePath);
            },
            null,
            checkFile),

/*************************
 * ...and a path handler *
 *************************/
    new Test(BASE + "/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              srv.registerPathHandler("/test_registerdirectory.js",
                                      override_test_registerdirectory);
            },
            checkOverride,
            null),

/************************
 * removed base handler *
 ************************/
    new Test(BASE + "/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              serverBasePath = null;
              srv.registerDirectory("/", serverBasePath);
            },
            checkOverride,
            null),

/************************
 * removed path handler *
 ************************/
    new Test(BASE + "/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              srv.registerPathHandler("/test_registerdirectory.js", null);
            },
            notFound,
            null),

/*************************
 * mapping set up, works *
 *************************/
    new Test(BASE + "/foo/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              serverBasePath = testsDirectory.clone();
              srv.registerDirectory("/foo/", serverBasePath);
            },
            check200,
            null),

/*********************
 * no mapping, fails *
 *********************/
    new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
            nocache,
            notFound,
            null),

/******************
 * mapping, works *
 ******************/
    new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              srv.registerDirectory("/foo/test_registerdirectory.js/",
                                    serverBasePath);
            },
            null,
            checkFile),

/************************************
 * two mappings set up, still works *
 ************************************/
    new Test(BASE + "/foo/test_registerdirectory.js",
                nocache, null, checkFile),

/**************************
 * remove topmost mapping *
 **************************/
    new Test(BASE + "/foo/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              srv.registerDirectory("/foo/", null);
            },
            notFound,
            null),

/**************************************
 * lower mapping still present, works *
 **************************************/
    new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
                nocache, null, checkFile),

/*******************
 * mapping removed *
 *******************/
    new Test(BASE + "/foo/test_registerdirectory.js/test_registerdirectory.js",
            function(ch)
            {
              nocache(ch);
              srv.registerDirectory("/foo/test_registerdirectory.js/", null);
            },
            notFound,
            null)
  ];
});


var srv;
var serverBasePath;
var testsDirectory;

function run_test()
{
  testsDirectory = do_get_cwd();

  srv = createServer();
  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
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
