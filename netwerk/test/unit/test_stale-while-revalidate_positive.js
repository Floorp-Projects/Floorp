/*

Tests the Cache-control: stale-while-revalidate response directive.

Purpose is to check we perform the background revalidation when the window is set
and we hit it.

* Make request #1.
  - response is from the server and version=1
  - max-age=1, stale-while-revalidate=9999
* Switch version of the data on the server and prolong the max-age to not let req #3
  do a bck reval at the end of the test (prevent leaks/shutdown races.)
* Make request #2 in 2 seconds (entry should be expired by that time, but fall into
  the reval window.)
  - response is from the cache, version=1
  - a new background request should be made for the data
* Wait for "http-on-background-revalidation" notifying finish of the background reval.
* Make request #3.
  - response is from the cache, version=2
* Done.

*/

"use strict";

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

function run_test() {
  run_next_test();
}

let max_age;
let version;
let generate_response = ver => `response version=${ver}`;

function test_handler(metadata, response) {
  const originalBody = generate_response(version);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-control", `max-age=${max_age}, stale-while-revalidate=9999`, false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

async function get_response(channel, fromCache) {
  return new Promise(resolve => {
    channel.asyncOpen(new ChannelListener((request, buffer, ctx, isFromCache) => {
      ok(fromCache == isFromCache, `got response from cache = ${fromCache}`);
      resolve(buffer);
    }));
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

async function background_reval_promise() {
  return new Promise(resolve => {
    Services.obs.addObserver(resolve, "http-on-background-revalidation");
  });
}

add_task(async function() {
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

  await sleep(max_age + 1);

  // must specifically wait for the internal channel to finish the reval to make
  // the test race-free.
  let reval_done = background_reval_promise();

  version = 2;
  max_age = 100;
  response = await get_response(make_channel(URI), true);
  ok(response == generate_response(1), "got response ver 1");

  await reval_done;

  response = await get_response(make_channel(URI), true);
  ok(response == generate_response(2), "got response ver 2");

  await stop_server(httpserver);
});
