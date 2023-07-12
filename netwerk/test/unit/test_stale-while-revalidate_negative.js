/*

Tests the Cache-control: stale-while-revalidate response directive.

Purpose is to check we DON'T perform the background revalidation when we make the
request past the reval window.

* Make request #1.
  - response is from the server and version=1
  - max-age=1, stale-while-revalidate=1
* Switch version of the data on the server.
* Make request #2 in 3 seconds (entry should be expired by that time and no longer
  fall into the reval window.)
  - response is from the server, version=2
* Done.

*/

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let max_age;
let version;
let generate_response = ver => `response version=${ver}`;

function test_handler(metadata, response) {
  const originalBody = generate_response(version);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader(
    "Cache-control",
    `max-age=${max_age}, stale-while-revalidate=1`,
    false
  );
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
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

async function sleep(time) {
  return new Promise(resolve => {
    do_timeout(time * 1000, resolve);
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

  version = 1;
  max_age = 1;
  response = await get_response(make_channel(URI), false);
  ok(response == generate_response(1), "got response ver 1");

  await sleep(max_age + 1 /* stale window */ + 1 /* to expire the window */);

  version = 2;
  response = await get_response(make_channel(URI), false);
  ok(response == generate_response(2), "got response ver 2");

  await stop_server(httpserver);
});
