/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

Test that a response to HEAD method should not have a body.
1. Create a GET request and write the response into cache.
2. Create the second GET request with the same URI and see if the response is
   from cache.
3. Create a HEAD request and test if we got a response with an empty body.

*/

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const responseContent = "response body";

function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-control", "max-age=9999", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  if (metadata.method != "HEAD") {
    response.bodyOutputStream.write(responseContent, responseContent.length);
  }
}

function make_channel(url, method) {
  let channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  channel.requestMethod = method;
  return channel;
}

async function get_response(channel, fromCache) {
  return new Promise(resolve => {
    channel.asyncOpen(
      new ChannelListener((request, buffer, ctx, isFromCache) => {
        ok(fromCache == isFromCache, `got response from cache = ${fromCache}`);
        resolve(buffer);
      })
    );
  });
}

async function stop_server(httpserver) {
  return new Promise(resolve => {
    httpserver.stop(resolve);
  });
}

add_task(async function () {
  let httpserver = new HttpServer();
  httpserver.registerPathHandler("/testdir", test_handler);
  httpserver.start(-1);
  const PORT = httpserver.identity.primaryPort;
  const URI = `http://localhost:${PORT}/testdir`;

  let response;

  response = await get_response(make_channel(URI, "GET"), false);
  ok(response === responseContent, "got response body");

  response = await get_response(make_channel(URI, "GET"), true);
  ok(response === responseContent, "got response body from cache");

  response = await get_response(make_channel(URI, "HEAD"), false);
  ok(response === "", "should have empty body");

  await stop_server(httpserver);
});
