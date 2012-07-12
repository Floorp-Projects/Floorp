/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

onconnect = function(e) {
  let port = e.ports[0];
  port.onmessage = function onMessage(event) {
    let {topic, data} = event.data;
    switch (topic) {
      case "social.initialize":
        port.postMessage({topic: "social.initialize-response"});
        break;
      case "test-initialization":
        port.postMessage({topic: "test-initialization-complete"});
        break;
    }
  }
}
