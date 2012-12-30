/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// apiPort is our port to WorkerAPI
let apiPort;
// testerPort is whatever port a test calls us on
let testerPort;

onconnect = function(e) {
  // assume this is a test connecting, but if we get
  // social.initialize, we know it is our WorkerAPI
  // instance connecting and we'll set apiPort
  let port = e.ports[0];
  port.onmessage = function onMessage(event) {
    let {topic, data} = event.data;
    switch (topic) {
      case "social.initialize":
        apiPort = port;
        break;
      case "test-initialization":
        testerPort = port;
        port.postMessage({topic: "test-initialization-complete"});
        break;
      case "test-profile":
        apiPort.postMessage({topic: "social.user-profile", data: data});
        break;
      case "test-pending-msg":
        port.postMessage({topic: "test-pending-response"})
        break;
      case "test-ambient":
        apiPort.postMessage({topic: "social.ambient-notification", data: data});
        break;
      case "test.cookies-get":
        apiPort.postMessage({topic: "social.cookies-get"});
        break;
      case "social.cookies-get-response":
        testerPort.postMessage({topic: "test.cookies-get-response", data: data});
        break;
      case "test-reload-init":
        // browser_social_sidebar.js started test, tell the sidebar to
        // start
        apiPort.postMessage({topic: 'social.reload-worker'});
        break;
      case "test-notification-create":
        apiPort.postMessage({topic: 'social.notification-create',
                             data: data});
        testerPort.postMessage({topic: 'did-notification-create'});
        break;
    }
  }
  // used for "test-reload-worker"
  if (apiPort && apiPort != port) {
    port.postMessage({topic: "worker.connected"})
  }

}
