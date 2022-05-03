"use strict";

function handleRequest(request, response) {
  // increase count
  let count = JSON.parse(getSharedState("earlyHintCount"));
  if (
    request.hasHeader("X-Moz") &&
    request.getHeader("X-Moz") === "early hint"
  ) {
    count.hinted += 1;
  } else {
    count.normal += 1;
  }
  setSharedState("earlyHintCount", JSON.stringify(count));

  // respond with redirect
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  let location = request.queryString;
  response.setHeader("Location", location, false);
  response.write("Hello world!");
}
