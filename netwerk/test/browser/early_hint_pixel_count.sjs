"use strict";

function handleRequest(request, response) {
  if (request.hasHeader("X-Early-Hint-Count-Start")) {
    setSharedState("earlyHintCount", JSON.stringify({ hinted: 0, normal: 0 }));
  }
  response.setHeader("Content-Type", "application/json", false);
  response.write(getSharedState("earlyHintCount"));
}
