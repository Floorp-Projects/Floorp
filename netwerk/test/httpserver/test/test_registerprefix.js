/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the registerPrefixHandler API

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

function makeCheckOverride(magic)
{
  return (function checkOverride(ch)
  {
    do_check_eq(ch.responseStatus, 200);
    do_check_eq(ch.responseStatusText, "OK");
    do_check_true(ch.requestSucceeded);
    do_check_eq(ch.getResponseHeader("Override-Succeeded"), magic);
  });
}

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test(BASE + "/prefix/dummy", prefixHandler, null,
                makeCheckOverride("prefix")),
    new Test(BASE + "/prefix/dummy", pathHandler, null,
                makeCheckOverride("path")),
    new Test(BASE + "/prefix/subpath/dummy", longerPrefixHandler, null,
                makeCheckOverride("subpath")),
    new Test(BASE + "/prefix/dummy", removeHandlers, null, notFound),
    new Test(BASE + "/prefix/subpath/dummy", newPrefixHandler, null,
                makeCheckOverride("subpath"))
  ];
});

/***************************
 * registered prefix handler *
 ***************************/

function prefixHandler(channel)
{
  nocache(channel);
  srv.registerPrefixHandler("/prefix/", makeOverride("prefix"));
}

/********************************
 * registered path handler on top *
 ********************************/

function pathHandler(channel)
{
  nocache(channel);
  srv.registerPathHandler("/prefix/dummy", makeOverride("path"));
}

/**********************************
 * registered longer prefix handler *
 **********************************/

function longerPrefixHandler(channel)
{
  nocache(channel);
  srv.registerPrefixHandler("/prefix/subpath/", makeOverride("subpath"));
}

/************************
 * removed prefix handler *
 ************************/

function removeHandlers(channel)
{
  nocache(channel);
  srv.registerPrefixHandler("/prefix/", null);
  srv.registerPathHandler("/prefix/dummy", null);
}

/*****************************
 * re-register shorter handler *
 *****************************/

function newPrefixHandler(channel)
{
  nocache(channel);
  srv.registerPrefixHandler("/prefix/", makeOverride("prefix"));
}

var srv;
var serverBasePath;
var testsDirectory;

function run_test()
{
  testsDirectory = do_get_profile();

  srv = createServer();
  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}

// PATH HANDLERS

// generate an override
function makeOverride(magic)
{
  return (function override(metadata, response)
  {
    response.setStatusLine("1.1", 200, "OK");
    response.setHeader("Override-Succeeded", magic, false);

    var body = "success!";
    response.bodyOutputStream.write(body, body.length);
  });
}
