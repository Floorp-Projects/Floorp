/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function() {
  return new Promise(function(resolve) {
    window.addEventListener("message", function(event) {
      if (event.data[0] == "raptor-benchmark") {
        console.log("Benchmark data received for ", event.data[0]);
        let data = {
          [event.data[1]]: event.data.slice(2),
        };
        resolve(data);
      }
    });
  }).catch(function() {
    console.log("Benchmark Promise Rejected");
  });
})();
