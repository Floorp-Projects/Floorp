/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);
  let token = query.get("token");

  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Headers", "x-test-header", false);

  if (request.method == "OPTIONS") {
    response.setHeader(
      "Access-Control-Allow-Methods",
      request.getHeader("Access-Control-Request-Method"),
      false
    );
    response.setHeader("Access-Control-Max-Age", "20", false);

    setState(token, token);
  } else {
    let test_op = request.getHeader("x-test-header");

    if (test_op == "check") {
      let value = getState(token);

      if (value) {
        response.write("1");
        setState(token, "");
      } else {
        response.write("0");
      }
    }
  }
}
