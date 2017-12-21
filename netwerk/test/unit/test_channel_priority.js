/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Cu, Ci, Assert, run_next_test, add_test, do_register_cleanup */
/* globals runningInParent, do_send_remote_message */
/* globals ChannelListener */

'use strict';

/* globals NetUtil*/
Cu.import('resource://gre/modules/NetUtil.jsm');
/* globals HttpServer */
Cu.import('resource://testing-common/httpd.js');

let httpserver;
let port;

function startHttpServer() {
  httpserver = new HttpServer();

  httpserver.registerPathHandler('/resource', (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, 'OK');
    response.setHeader('Content-Type', 'text/plain', false);
    response.setHeader('Cache-Control', 'no-cache', false);
    response.bodyOutputStream.write("data", 4);
  });

  httpserver.registerPathHandler('/redirect', (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 302, 'Redirect');
    response.setHeader('Location', '/resource', false);
    response.setHeader('Cache-Control', 'no-cache', false);
  });

  httpserver.start(-1);
  port = httpserver.identity.primaryPort;
}

function stopHttpServer() {
  httpserver.stop(()=>{});
}

function makeRequest(uri) {
  let requestChannel = NetUtil.newChannel({uri, loadUsingSystemPrincipal: true});
  requestChannel.QueryInterface(Ci.nsISupportsPriority);
  requestChannel.priority = Ci.nsISupportsPriority.PRIORITY_HIGHEST;
  requestChannel.asyncOpen2(new ChannelListener(checkResponse, requestChannel));
}

function checkResponse(request, buffer, requestChannel) {
  requestChannel.QueryInterface(Ci.nsISupportsPriority);
  Assert.equal(requestChannel.priority, Ci.nsISupportsPriority.PRIORITY_HIGHEST);

  // the response channel can be different (if it was redirected)
  let responseChannel = request.QueryInterface(Ci.nsISupportsPriority);
  Assert.equal(responseChannel.priority, Ci.nsISupportsPriority.PRIORITY_HIGHEST);

  run_next_test();
}

add_test(function test_regular_request() {
  makeRequest(`http://localhost:${port}/resource`);
});

add_test(function test_redirect() {
  makeRequest(`http://localhost:${port}/redirect`);
});

function run_test() { // jshint ignore:line
  if (!runningInParent) {
    // add a task to report test finished to parent process at the end of test queue,
    // since do_register_cleanup is not available in child xpcshell test script.
    add_test(function () {
      do_send_remote_message('finished');
      run_next_test();
    });

    // waiting for parent process to assign server port via configPort()
    return;
  }

  startHttpServer();
  registerCleanupFunction(stopHttpServer);
  run_next_test();
}

// This is used by unit_ipc/test_channel_priority_wrap.js for e10s XPCShell test
function configPort(serverPort) { // jshint ignore:line
  port = serverPort;
  run_next_test();
}
