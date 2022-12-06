"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "image/png", false);
  response.setHeader("Cache-Control", "max-age=604800", false);

  // the typo in "Referer" is part of the http spec
  if (request.hasHeader("Referer")) {
    setSharedState("requestReferrer", request.getHeader("Referer"));
  } else {
    setSharedState("requestReferrer", "");
  }

  let count = JSON.parse(getSharedState("earlyHintCount"));
  let image;
  // send different sized images depending whether this is an early hint request
  if (
    request.hasHeader("X-Moz") &&
    request.getHeader("X-Moz") === "early hint"
  ) {
    count.hinted += 1;
    // set to green/black horizontal stripes (71 bytes)
    image = atob(
      "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAADklEQVQIW2OU+i/FAAcADoABNV8X" +
        "GBMAAAAASUVORK5CYII="
    );
  } else {
    count.normal += 1;
    // set to purple/white checkered pattern (76 bytes)
    image = atob(
      "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAE0lEQVQIW2P4//+/N8MkBiAGsgA1" +
        "bAe1SzDY8gAAAABJRU5ErkJggg=="
    );
  }
  setSharedState("earlyHintCount", JSON.stringify(count));
  response.write(image);
}
