/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/CloudSync.jsm");

function run_test () {
  run_next_test();
}

add_task(function* test_get_remote_tabs () {
  let cloudSync = CloudSync();
  let clients = yield cloudSync.tabs.getRemoteTabs();
  equal(clients.length, 0);

  yield cloudSync.tabs.mergeRemoteTabs({
      id: "001",
      name: "FakeClient",
    },[
      {url:"https://www.google.ca?a=å%20ä%20ö",title:"Google Canada",icon:"https://www.google.ca/favicon.ico",lastUsed:0},
      {url:"http://www.reddit.com",title:"Reddit",icon:"http://www.reddit.com/favicon.ico",lastUsed:1},
    ]);
  ok(cloudSync.tabs.hasRemoteTabs());

  clients = yield cloudSync.tabs.getRemoteTabs();
  equal(clients.length, 1);
  equal(clients[0].tabs.size, 2);
});
