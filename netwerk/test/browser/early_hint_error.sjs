"use strict";

function handleRequest(request, response) {
  response.setStatusLine(
    request.httpVersion,
    parseInt(request.queryString),
    "Dynamic error"
  );
  response.setHeader("Content-Type", "image/png", false);
  response.setHeader("Cache-Control", "max-age=604800", false);

  // count requests
  let image;
  let count = JSON.parse(getSharedState("earlyHintCount"));
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
