/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";
Cu.importGlobalProperties(["URLSearchParams", "URL"]);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  if (params.has("no_redirect")) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("ok");
  } else {
    if (params.has("delay")) {
      response.processAsync();
      setTimeout(() => response.finish(), params.get("delay"));
    }
    if (request.method == "POST") {
      response.setStatusLine(request.httpVersion, 303, "Redirected");
    } else {
      response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
    }
    let url = new URL(
      params.get("redirect_uri") || params.get("default_redirect")
    );
    url.searchParams.set("access_token", "here ya go");
    response.setHeader("Location", url.href);
  }
}
