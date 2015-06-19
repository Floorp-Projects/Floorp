/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch on http:// URI's.

const { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});

const server = new HttpServer();
server.registerDirectory("/", do_get_cwd());
server.registerPathHandler("/cached.json", cacheRequestHandler);
server.start(-1);

const port = server.identity.primaryPort;
const serverURL = "http://localhost:" + port;
const CACHED_URL = serverURL + "/cached.json";
const NORMAL_URL = serverURL + "/test_fetch-http.js";

function cacheRequestHandler(request, response) {
  do_print("Got request for " + request.path);
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);

  let body = "[" + Math.random() + "]";
  response.bodyOutputStream.write(body, body.length);
}

do_register_cleanup(() => {
  return new Promise(resolve => server.stop(resolve));
});

add_task(function* test_normal() {
  yield DevToolsUtils.fetch(NORMAL_URL).then(({content}) => {
    ok(content.contains("The content looks correct."),
      "The content looks correct.");
  });
});

add_task(function* test_caching() {
  let initialContent = null;

  do_print("Performing the first request.");
  yield DevToolsUtils.fetch(CACHED_URL).then(({content}) => {
    do_print("Got the first response: " + content);
    initialContent = content;
  });

  do_print("Performing another request, expecting to get cached response.");
  yield DevToolsUtils.fetch(CACHED_URL).then(({content}) => {
    deepEqual(content, initialContent, "The content was loaded from cache.");
  });

  do_print("Performing a third request with cache bypassed.");
  let opts = { loadFromCache: false };
  yield DevToolsUtils.fetch(CACHED_URL, opts).then(({content}) => {
    notDeepEqual(content, initialContent,
      "The URL wasn't loaded from cache with loadFromCache: false.");
  });
});
