// A 1x1 PNG image.
// Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
const IMAGE = atob("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
                   "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=");

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  if (aRequest.queryString.includes("result")) {
    aResponse.write(getState("hints") || 0);
    setState("hints", "0");
  } else {
    let hints = parseInt(getState("hints") || 0) + 1;
    setState("hints", hints.toString());

    aResponse.setHeader("Cache-Control", "max-age=10000", false);
    aResponse.setHeader("Content-Type", "image/png", false);
    aResponse.write(IMAGE);
 }
}
