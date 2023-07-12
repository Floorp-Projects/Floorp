/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Test if 204 response is cached.
1. Make first http request and return a 204 response.
2. Check if the first response is not cached.
3. Make second http request and check if the response is cached.

*/

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-control", "max-age=9999", false);
  response.setStatusLine(metadata.httpVersion, 204, "No Content");
}

function make_channel(url) {
  let channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  return channel;
}

async function get_response(channel, fromCache) {
  return new Promise(resolve => {
    channel.asyncOpen(
      new ChannelListener((request, buffer, ctx, isFromCache) => {
        ok(fromCache == isFromCache, `got response from cache = ${fromCache}`);
        resolve();
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

  await get_response(make_channel(URI, "GET"), false);
  await get_response(make_channel(URI, "GET"), true);

  await stop_server(httpserver);
});
